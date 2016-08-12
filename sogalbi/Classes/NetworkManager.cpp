#include "pch.h"
#include "NetworkManager.h"
#include "Ws2tcpip.h"

NetworkManager* NetworkManager::_instance = nullptr;

NetworkManager::NetworkManager()
{
	initTcp();
}

void NetworkManager::initTcp()
{
	// ���� �ʱ�ȭ
	WSAData wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		log("WSAStartup failed");
		return;
	}

	// socket()
	_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (_sock == INVALID_SOCKET)
	{
		log("socket() failed");
		return;
	}
}

// ������ true, ���н� false ��ȯ
bool NetworkManager::connectTcp(std::string serverIp, int serverPort)
{
	SOCKADDR_IN serverAddr;
	ZeroMemory(&serverAddr, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	inet_pton(AF_INET, serverIp.c_str(), static_cast<void*>(&serverAddr.sin_addr.s_addr));
	serverAddr.sin_port = htons(serverPort);
	
	auto result = connect(_sock,(SOCKADDR*)&serverAddr, sizeof(serverAddr));
	if (result == SOCKET_ERROR)
		return false;
	return true;
}
