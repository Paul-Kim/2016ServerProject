#include "stdafx.h"
#include "IOCPManager.h"


IOCPManager* IOCPManager::_instance = nullptr;

void IOCPManager::WorkerThreadFunc()
{

}

void IOCPManager::ListenThreadFunc()
{

}

void IOCPManager::InitServer(ServerSetting& setting)
{
	// ���� �ʱ�ȭ
	auto wsaData = WSADATA{};
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		Beep(1000,1000); // TODO : ���� �߻��� �ϴ� beep �صξ����� 


	// IOCP�� �����.
	_completionPort = CreateIOCP();

	// worker ������ Ǯ�� �����.
	auto sysInfo = SYSTEM_INFO{};
	GetSystemInfo(&sysInfo); // �ý��� ���� �˾ƿ���. �ھ� ���� �ľ��� ����
	auto numThread = sysInfo.dwNumberOfProcessors*2; // �ھ� ���� * 2����ŭ worker ������ Ǯ�� �����.
	for (auto i = 0u; i < numThread; ++i)
	{
		//std::thread(WorkerThreadFunc);
	}


	// ���� ���� ����
	// TODO : SOCK_STREAM, WSA_FLAG_OVERLAPPED�� �ǹ̴� �� �𸣰ڴ�. �ּ� �ٶ�
	_serverSocket = WSASocket(AF_INET, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
	sockaddr_in socketAddress;
	ZeroMemory(&socketAddress, sizeof(socketAddress));
	socketAddress.sin_family = AF_INET;
	socketAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	socketAddress.sin_port = htons(setting._portNum);
	bind(_serverSocket, (sockaddr*)&socketAddress, sizeof(socketAddress));

}

HANDLE IOCPManager::CreateIOCP()
{
	return CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
}

void IOCPManager::BindSocketToIOCP()
{
	
}

IOCPManager* IOCPManager::GetInstance()
{
	if (_instance == nullptr)
		_instance = new IOCPManager();
	return _instance;
}

void IOCPManager::StartServer()
{
	
}
