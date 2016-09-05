#include "pch.h"
#include "..\..\Common\Common.h"
#include "Ws2tcpip.h"
#include "NetworkManager.h"
#include "ClientLogger.h"

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

// ��Ŷ ť���� �� �ϳ��� ���� �����Ѵ�
void NetworkManager::recvAndGivePacketsToCallbackFuncThread()
{
	using namespace COMMON;

	// ���ۿ� �����ִ� �������� ��. �ϰ����� �����ַ��� �ʱⰪ�� �翬�� 0
	auto remainingDataSizeInRecvBuffer = 0;

	while (true)
	{
		// ���� ������ �������� �˾Ƽ� recv �õ��� �׸��д�.
		if (_sock <= 0)
			break;
		// ���� �� ���� ��ŭ �����͸� �޴´�. �̶� ä��� �����ϴ� ��ġ�� �����ִ� �������� �ٷ� ��
		auto receivedSize = recv(_sock, _recvBuffer + remainingDataSizeInRecvBuffer, sizeof(_recvBuffer), 0);
		if (receivedSize == SOCKET_ERROR)
			return;


		// ���ۿ� �����ִ� �������� ��. �ϰ����� �����ַ��� recv�� ��ŭ �÷��ִ� ���� �翬.
		remainingDataSizeInRecvBuffer += receivedSize;
		// �������� ���� ��Ŷ ��ġ�� recv���� �� �պ���.
		auto nextPacketAddress = _recvBuffer;

		while (true)
		{
			// ����� ���� �� ���� ��
			if (remainingDataSizeInRecvBuffer >= PACKET_HEADER_SIZE)
			{
				// ����� �����.
				auto packetHeader = (PacketHeader*)nextPacketAddress;
				// �ٵ� ����⿡�� ����� ��
				if (remainingDataSizeInRecvBuffer >= packetHeader->_bodySize + PACKET_HEADER_SIZE)
				{
					static bool processDone = false;

					// ���μ��� �Լ��� ó���� ������ ��ٸ�
					processDone = false;
					auto processLambda = [&]() {
						_callbackFunc(packetHeader->_id, packetHeader->_bodySize, nextPacketAddress + PACKET_HEADER_SIZE);
						processDone = true;
					};

					// ������� �Դٸ� ��Ŷ�� ���� �� �ִ� ��. ��Ŷ�� ��Ź�� �ʿ� �ѱ��.
					Director::getInstance()->getScheduler()->performFunctionInCocosThread(processLambda);

					while (processDone == false)
						std::this_thread::sleep_for(std::chrono::milliseconds(0));


					// ������ ���� ��Ŷ�� �ּҴ� ���� ���� ��Ŷ�� �ٷ� ���̴�.
					nextPacketAddress += PACKET_HEADER_SIZE + packetHeader->_bodySize;
					// ����, �ϳ� ��������� ���� ������ ���� �پ����ٴ� ���� ǥ���Ѵ�.
					remainingDataSizeInRecvBuffer -= PACKET_HEADER_SIZE + packetHeader->_bodySize;
					continue; // �ϳ� ��������� ���� ��Ŷ�� ���鷯 ����Ѵ�.
				}
			}
			// ���� �Դٴ� ���� ��Ŷ�� ����⿡ �����Ͱ� ���ڶ�ٴ� ��.
			// ���� ���� ������� ��Ŷ���� callbackFunc()�� ���� ��ȭ�ߴٴ� ��.
			// ���� ��Ḧ recv������ ���� ������ ���� ������ Recv�� ��ٸ���.
			memcpy(_recvBuffer, nextPacketAddress, remainingDataSizeInRecvBuffer);
			break;
		}
		// ���� �� ���� ��ŭ �� ��������Ƿ� ������ Recv�� ��ٸ���.
	}
}



void NetworkManager::setRecvCallback(std::function<void(const COMMON::PACKET_ID, const short, char*)> callbackFunc)
{
	_callbackFunc = callbackFunc;
}

void NetworkManager::disconnectTcp()
{
	closesocket(_sock);
}

// send() �������� ��ȯ
bool NetworkManager::sendPacket(const COMMON::PACKET_ID packetId, const short dataSize, char* pData)
{
	_mutex.lock();

	char data[COMMON::MAX_PACKET_SIZE] = { 0, };

	// ���
	COMMON::PacketHeader packetHeader{ packetId,dataSize };
	memcpy(data, (char*)&packetHeader, COMMON::PACKET_HEADER_SIZE);

	// ������
	if (dataSize > 0)
		memcpy(data + COMMON::PACKET_HEADER_SIZE, pData, dataSize);

	// ����
	auto result = send(_sock, data, dataSize + COMMON::PACKET_HEADER_SIZE, 0);
	if (result == SOCKET_ERROR)
	{
		ClientLogger::logThreadSafe("send() failed. packet id : " + packetId);
		// disconnect() ������ҵ�
		_mutex.unlock();
		return false;
	}

	ClientLogger::logThreadSafe("packet send success. packet id : " + packetId);
	_mutex.unlock();
	return true;
}

NetworkManager* NetworkManager::getInstance()
{
	if (_instance == nullptr)
		_instance = new NetworkManager();
	return _instance;
}

// ������ true, ���н� false ��ȯ
bool NetworkManager::connectTcp(std::string serverIp, int serverPort)
{
	auto returnVal = true;
	if (_mutex.try_lock() == false)
		return false;
	SOCKADDR_IN serverAddr;
	ZeroMemory(&serverAddr, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	inet_pton(AF_INET, serverIp.c_str(), static_cast<void*>(&serverAddr.sin_addr.s_addr));
	serverAddr.sin_port = htons(serverPort);

	auto result = connect(_sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
	if (result == SOCKET_ERROR)
	{
		ClientLogger::msgBox(L"�ش� ä���� �������� �ʽ��ϴ�.");
		ClientLogger::logThreadSafe("connect() failed");
		returnVal = false;
	}
	// TODO : ¥�� ��
	auto newThread = std::thread(std::bind(&NetworkManager::recvAndGivePacketsToCallbackFuncThread, this));
	newThread.detach();
	_mutex.unlock();
	return returnVal;
}