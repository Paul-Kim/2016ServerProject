#include "stdafx.h"
#include "NetworkSetting.h"
#include "BufferQueue.h"
#include <vector>

#include "IOCPManager.h"


IOCPManager* IOCPManager::_instance = nullptr;

// ä�� ������ Json���� �о�´�
// TODO : ���� ������ �־����. json���� �о���� ����� ���ľ� ��
void IOCPManager::LoadChannelSettingFromJson()
{
	auto setting = NetworkSetting{};
	setting._maxBufferCount = 10000;
	setting._maxBufferSize = 1024 * 4; // byte
	setting._maxSessionCount = 5000;
	setting._portNum = 34343;
	
	_setting = setting;
}

void IOCPManager::WorkerThreadFunc()
{
	DWORD transferedByte = 0;
	unsigned sessionIndex = 0u;
	IOInfo ioInfo;

	while (true)
	{
		GetQueuedCompletionStatus(_completionPort, &transferedByte,
			(PULONG_PTR)&sessionIndex, (LPOVERLAPPED*)&ioInfo, INFINITE);
		
		auto sock = _sessionPool[sessionIndex]._socket;

		if (ioInfo._rwMode == IOInfo::RWMode::READ)
		{

		}
		else
		{

		}
	}
}

void IOCPManager::ListenThreadFunc()
{
	while (true)
	{

		auto clientAddr = sockaddr_in{};
		int addrLen = sizeof(clientAddr);
		// ���⼭ block
		auto newSocket = accept(_serverSocket, (SOCKADDR*)&clientAddr, &addrLen);


		// ������ �ʰ�..
		while (_sessionIndexPool.empty())
			Sleep(1000); // TODO : ������ �ʰ��� ó��. �ϴ��� �ڸ� �������� ��ٸ�

		// ���� ������ ������ ����� ������ ��� �����߿� ���´�.
		_sessionPoolMutex.lock();
			auto newSessionIndex = _sessionIndexPool.front();
		_sessionPoolMutex.unlock();
		auto& newSession = _sessionPool.at(newSessionIndex);
		ZeroMemory(&newSession, sizeof(newSession));
		// ���ǿ� ���ο� Ŭ�� ���� ����
		newSession._sockAddr = clientAddr;
		newSession._socket = newSocket;


		auto ioInfo = new IOInfo();
		// �ش� io�� ����� ���۸� ���� Ǯ���� �ϳ� ���´�.
		ioInfo->_wsaBuf.buf = BufferQueue::GetInstance()->GetBufferThreadSafe();
		ioInfo->_wsaBuf.len = _setting._maxBufferSize;
		ioInfo->_rwMode = IOInfo::RWMode::READ;
		ZeroMemory(&ioInfo->_overlapped, sizeof(OVERLAPPED));

		// Completion Port�� ���ο� ������ �����
		BindSessionToIOCP(newSession);

		DWORD recvSize = 0, flags = 0;

		// Recv �ɾ���´�. �Ϸ�Ǹ� worker thread�� �Ѿ
		WSARecv(newSession._socket,	// ����
			&ioInfo->_wsaBuf,		// �ش� recv�� ����� ����
			1,						// ����� ���� ����
			&recvSize,&flags,&ioInfo->_overlapped,nullptr);
	}
}

void IOCPManager::StartServer()
{
	// ä�� ���� �о���� �κ�
	LoadChannelSettingFromJson();


	// IOCP�� �����.
	_completionPort = CreateIOCP();

	// ���� Ǯ�� �����.
	CreateSessionPool();

	// ���� �ʱ�ȭ
	auto wsaData = WSADATA{};
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		Beep(1000, 1000); // TODO : ���� �߻��� �ϴ� beep �ص�. ���߿� ����ó�� �ʿ�
	// ���� ���� ����
	// TODO : SOCK_STREAM, WSA_FLAG_OVERLAPPED�� �ǹ̴� �� �𸣰ڴ�. �ּ� �ٶ�
	_serverSocket = WSASocket(AF_INET, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
	sockaddr_in socketAddress;
	ZeroMemory(&socketAddress, sizeof(socketAddress));
	socketAddress.sin_family = AF_INET;
	socketAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	socketAddress.sin_port = htons(_setting._portNum);

	bind(_serverSocket, (sockaddr*)&socketAddress, sizeof(socketAddress));
	// ���� ���� Ȱ��ȭ
	// TODO : backlog ������ ���� �ʿ�
	listen(_serverSocket, 5);

	// listen �����带 �����.
	auto listenThread = std::thread(std::bind(&IOCPManager::ListenThreadFunc, this));
	listenThread.detach();


	// worker ������ Ǯ�� �����.
	auto sysInfo = SYSTEM_INFO{};
	GetSystemInfo(&sysInfo); // �ý��� ���� �˾ƿ���. �ھ� ���� �ľ��� ����
	auto numThread = sysInfo.dwNumberOfProcessors * 2;
	// �ھ� ���� * 2����ŭ worker ������ Ǯ�� �����.
	for (auto i = 0u; i < numThread; ++i)
	{
		// �� Ŭ���̾�Ʈ�� ���� worker ������
		auto workerThread = std::thread(std::bind(&IOCPManager::WorkerThreadFunc,this));
		workerThread.detach();
	}
}

HANDLE IOCPManager::CreateIOCP()
{
	return CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
}

void IOCPManager::CreateSessionPool()
{
	// ����ڵ�
	if (_sessionPool.size() != 0)
		return;

	for (unsigned i = 0; i < _setting._maxSessionCount; ++i)
	{
		_sessionPool.emplace_back(std::move(SessionInfo{}));
		_sessionIndexPool.push_back(i);
	}
}

void IOCPManager::BindSessionToIOCP(SessionInfo& targetSession)
{
	CreateIoCompletionPort((HANDLE)targetSession._socket, _completionPort, (ULONG_PTR)&targetSession._index, 0);
}

IOCPManager* IOCPManager::GetInstance()
{
	if (_instance == nullptr)
		_instance = new IOCPManager();
	return _instance;
}

void IOCPManager::DelInstance()
{
	if (_instance != nullptr)
	{
		delete _instance;
		_instance = nullptr;
	}
	return;
}
