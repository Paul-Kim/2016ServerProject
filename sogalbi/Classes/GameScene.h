#pragma once
#include "..\Common\Packet.h"

class Player;

class GameScene : public cocos2d::Layer
{
public:
    static cocos2d::Scene* createScene(int roomNum);

    virtual bool init(int roomNum);
    
    void menuCloseCallback(cocos2d::Ref* pSender);
	static GameScene* create(int roomNum);

private:
	void initLayout(int roomNum);
	void recvPacketProcess(COMMON::PACKET_ID packetId, short bodySize, char* bodyPos);
	void packetProcess_RoomEnterUserListRes(COMMON::RecvPacketInfo packetInfo);
public:
private:
	enum Z_ORDER : short
	{
		ERR = -1,
		BACKGROUND = 0,
		CARD_LOW = 1,
		CARD_TOP = 2,
		NAMETAG_BG = 3,
		NAMETAG_TOP = 4,
		UI_TOP = 10
	};
	Player* _players[5];
	COMMON::DealerInfo _dealerInfo;
	COMMON::UserInfo _userInfo[4];
};