#include "stdafx.h"
#include <vector>

#include "ServerConfig.h"
#include "NetworkSetting.h"
#include "BufferQueue.h"
#include "PacketQueue.h"

#include "IOCPManager.h"


IOCPManager* IOCPManager::_instance = nullptr;

void IOCPManager::LoadChannelSettingFromServerConfig(ServerConfig* serverconfig)
{
	auto setting = NetworkSetting{};
	setting._maxSessionCount = serverconfig->MAX_USERCOUNT_PER_CHANNEL + serverconfig->ExtraClientCount;
	setting._portNum = serverconfig->Port;
	setting._backLog = serverconfig->BackLogCount;
	setting._maxSessionRecvBufferSize = serverconfig->MAX_SESSION_RECV_BUFFER_SIZE;

	_setting = setting;
}

void IOCPManager::WorkerThreadFunc()
{
	DWORD transferedByte = 0;
	int *a = nullptr;
	IOInfo* ioInfo;

	while (true)
	{
		GetQueuedCompletionStatus(_completionPort, &transferedByte,
			(PULONG_PTR)&a, (LPOVERLAPPED*)&ioInfo, INFINITE);

		auto sessionIndex = ioInfo->_sessionIndex;
		SessionInfo* pSession = _sessionPool.at(sessionIndex);

		if (ioInfo->_rwMode == IOInfo::RWMode::READ)
		{
			auto headerPos = pSession->_recvBuffer;
			auto receivePos = ioInfo->_wsaBuf.buf;
			// ���� ó�� �ȵ� ������ �� = ������ - ��������
			auto totalDataSizeInBuf = receivePos + transferedByte - pSession->_recvBuffer;
			// ��Ŷ���� ��������� ��ٸ��� �������� ������
			auto remainingDataSizeInBuf = totalDataSizeInBuf;

			// ��Ŷ ���� ����
			while (remainingDataSizeInBuf >= PACKET_HEADER_SIZE)
			{ // ����� �鿩�� ���⿡ ����� �����Ͱ� �ִٸ�
				// ����� �鿩�ٺ���
				auto header = (PacketHeader*)headerPos;
				auto bodySize = header->_bodySize;
				// ��Ŷ �ϳ��� ������ ���� �� ���� ��
				if (PACKET_HEADER_SIZE + bodySize >= remainingDataSizeInBuf)
				{
					// ��Ŷ�� �����.
					auto newPacketInfo = RecvPacketInfo{};
					newPacketInfo.PacketId = header->_id;
					newPacketInfo.PacketBodySize = bodySize;
					newPacketInfo.pRefData = headerPos + PACKET_HEADER_SIZE; // �ٵ� ��ġ
					newPacketInfo.SessionIndex = sessionIndex;

					// ��Ŷť�� ����ִ´�. ����� �����ϹǷ� ��������.
					_recvPacketQueue->PushBack(newPacketInfo);

					// ��Ŷ�� �ϳ� ������ ��������Ƿ� ������ ��� �ڸ��� �����ϰ�, ���� ������ ����� �������ش�.
					headerPos += PACKET_HEADER_SIZE + bodySize;
					remainingDataSizeInBuf -= PACKET_HEADER_SIZE + bodySize;
				}
				else // ����� �ִµ� �����Ͱ� ���ڶ� ��Ŷ �ϳ��� ��¥�� ���� �� ���� ��
					break;
			}
			// ���� �� �ִ� ����� �� ��Ŷ���� �������. ���� ������ ������ �� ������ �������.
			// ���� �����̶�� �ϴ��� ���ۺκ��� ������ �������� ���̴�.
			memcpy_s(pSession->_recvBuffer, _setting._maxSessionRecvBufferSize, headerPos, remainingDataSizeInBuf);

			// ���� ���� �� �ִ� ��Ŷ�� �� �������. Recv �ɾ�����.
			ZeroMemory(&ioInfo->_overlapped, sizeof(OVERLAPPED));
			ioInfo->_wsaBuf.buf = pSession->_recvBuffer + remainingDataSizeInBuf;// ���� �̿� ��Ŷ�� ���� �� �����Ƿ� recv�� �̿���Ŷ �ڿ��ٰ� �޴´�.
			ioInfo->_wsaBuf.len = _setting._maxSessionRecvBufferSize - remainingDataSizeInBuf;
			ioInfo->_rwMode = IOInfo::RWMode::READ;
			DWORD recvSize = 0, flags = 0;
			WSARecv(pSession->_socket,	// ����
				&ioInfo->_wsaBuf,		// �ش� recv�� ����� ����
				1,						// ����� ���� ����
				&recvSize, &flags, &ioInfo->_overlapped, nullptr);
		}
		else // RWMode::WRITE
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
		_sessionIndexPool.pop_front();
		_sessionPoolMutex.unlock();
		auto newSession = _sessionPool.at(newSessionIndex);
		// ���ǿ� ���ο� Ŭ�� ���� ����
		newSession->_sockAddr = clientAddr;
		newSession->_socket = newSocket;


		auto ioInfo = new IOInfo();
		ZeroMemory(&ioInfo->_overlapped, sizeof(OVERLAPPED));
		ioInfo->_wsaBuf.buf = newSession->_recvBuffer;
		ioInfo->_wsaBuf.len = _setting._maxSessionRecvBufferSize;
		ioInfo->_rwMode = IOInfo::RWMode::READ;
		ioInfo->_sessionIndex = newSessionIndex;

		// Completion Port�� ���ο� ������ �����
		BindSessionToIOCP(newSession);

		DWORD recvSize = 0, flags = 0;

		// Recv �ɾ���´�. �Ϸ�Ǹ� worker thread�� �Ѿ
		WSARecv(newSession->_socket,	// ����
			&ioInfo->_wsaBuf,		// �ش� recv�� ����� ����
			1,						// ����� ���� ����
			&recvSize, &flags, &ioInfo->_overlapped, nullptr);
	}
}

// TODO : ��ȿ�������� ��� ������ ���� �ִµ�, ��Ŷ ť�� �����Ͱ� ���� �� WsaSend�� ȣ���ϰ� �ٲ� ����
void IOCPManager::SendThreadFunc()
{
	while (true)
	{
		if (_sendPacketQueue->IsEmpty())
		{
			// �켱 �۾��� �ִ� �ٸ� �����尡 ������ �纸�Ѵ�
			std::this_thread::sleep_for(std::chrono::milliseconds(0));
			continue;
		}
		// send�� �ص� �Ǵ� ��Ȳ���� Ȯ���ؼ� �ȵǴ� ��Ȳ�̸� �������� �̷��.
		auto packetToSend = _sendPacketQueue->ReadFront();
		auto targetSession = _sessionPool.at(packetToSend.SessionIndex);
		auto headerToSend = PacketHeader{(PACKET_ID)packetToSend.PacketId,packetToSend.PacketBodySize};
		send(targetSession->_socket, (char*)&headerToSend, PACKET_HEADER_SIZE, 0);
		send(targetSession->_socket, packetToSend.pRefData, packetToSend.PacketBodySize, 0);
		_sendPacketQueue->PopFront();
	}
}

COMMON::ERROR_CODE IOCPManager::StartServer()
{
	if (_initialized == false)
		return ERROR_CODE::IOCP_START_FAIL_NOT_INITIALIZED;

	// ���� ���� Ȱ��ȭ
	listen(_serverSocket, _setting._backLog);

	// listen ������ ������.
	auto listenThread = std::thread(std::bind(&IOCPManager::ListenThreadFunc, this));
	listenThread.detach();

	// worker ������ ������.
	auto sysInfo = SYSTEM_INFO{};
	GetSystemInfo(&sysInfo); // �ý��� ���� �˾ƿ���. �ھ� ���� �ľ��� ����
	auto numThread = sysInfo.dwNumberOfProcessors * 2;
	// �ھ� ���� * 2����ŭ worker ������ Ǯ�� �����.
	for (auto i = 0u; i < numThread; ++i)
	{
		// �� Ŭ���̾�Ʈ�� ���� worker ������
		auto workerThread = std::thread(std::bind(&IOCPManager::WorkerThreadFunc, this));
		workerThread.detach();
	}

	// send ������ ������.
	auto sendThread = std::thread(std::bind(&IOCPManager::SendThreadFunc, this));
	sendThread.detach();

	return ERROR_CODE::NONE;
}

HANDLE IOCPManager::CreateIOCP()
{
	return CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
}

void IOCPManager::CreateSessionPool()
{
	// ����ڵ�
	if (_sessionPool.empty() == false)
		return;

	for (unsigned i = 0; i < _setting._maxSessionCount; ++i)
	{
		auto newSession = new SessionInfo();
		newSession->_index = i;
		// ���Ǹ��� ������ �ִ� ���۸� ���� �����
		newSession->_recvBuffer = new char[_setting._maxSessionRecvBufferSize];
		_sessionPool.push_back(newSession);
		_sessionIndexPool.push_back(i);
	}
}

void IOCPManager::ReleaseSessionPool()
{
	for (auto& i : _sessionPool)
	{
		delete[] i->_recvBuffer;
		// TODO :: Session pool ������
	}
}

void IOCPManager::BindSessionToIOCP(SessionInfo* targetSession)
{
	CreateIoCompletionPort((HANDLE)targetSession->_socket, _completionPort, (ULONG_PTR)nullptr, 0);
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

COMMON::ERROR_CODE IOCPManager::InitServer(PacketQueue* recvPacketQueue, PacketQueue* sendPacketQueue, ServerConfig* serverConfig)
{
	if (recvPacketQueue == nullptr || sendPacketQueue == nullptr)
		return ERROR_CODE::IOCP_INIT_FAIL_DONOT_GIVEME_NULLPTR;

	// ������������ ä�μ����� �ҷ��´�
	LoadChannelSettingFromServerConfig(serverConfig);
	_recvPacketQueue = recvPacketQueue;
	_sendPacketQueue = sendPacketQueue;

	// ���� Ǯ�� �����.
	CreateSessionPool();

	// IOCP�� �����.
	_completionPort = CreateIOCP();

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

	_initialized = true;
	return ERROR_CODE::NONE;
}