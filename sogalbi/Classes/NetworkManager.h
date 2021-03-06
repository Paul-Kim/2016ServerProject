#pragma once

class NetworkManager
{
public:
	static NetworkManager* getInstance();

	bool	connectTcp(std::string serverIp, int serverPort);

	bool	sendPacket(const COMMON::PACKET_ID packetId, const short dataSize, char* pData);
	void	setRecvCallback(std::function<void(const COMMON::PACKET_ID, const short, char*)> callbackFunc);
	void	disconnectTcp();


private:
	NetworkManager();
	void	initTcp();
	void	recvAndGivePacketsToCallbackFuncThread();

private:
	static NetworkManager* _instance;
	std::mutex	_mutex;
	SOCKET _sock;
	std::string	_serverIp;
	int			_port;
	char		_recvBuffer[4096]; // 적정값 결정 필요
	std::function<void(const COMMON::PACKET_ID, const short, char*)> _callbackFunc;

	bool	_disConnected = false;
};