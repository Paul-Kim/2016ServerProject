#pragma once
#include "..\Common\Packet.h"

class Player;
class BetSlider;
class Hand;
class Dealer;

namespace cocos2d
{
	namespace ui
	{
		class Slider;
	}
}

class GameScene : public Layer
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
	void packetProcess_RoomEnterUserNtf(COMMON::RecvPacketInfo packetInfo);
	void packetProcess_RoomLeaveUserNtf(COMMON::RecvPacketInfo packetInfo);

	void packetProcess_GameBetCounter(COMMON::RecvPacketInfo packetInfo);
	void packetProcess_GameBetNtf(COMMON::RecvPacketInfo packetInfo);

	void packetProcess_GameStartNtf(COMMON::RecvPacketInfo packetInfo);
	void packetProcess_GameChangeTurnNtf(COMMON::RecvPacketInfo packetInfo);
	void packetProcess_GameChoiceNtf(COMMON::RecvPacketInfo packetInfo);
	void packetProcess_GameDealerResultNtf(COMMON::RecvPacketInfo packetInfo);

	bool betButtonClicked(Ref* sender);

	bool splitButtonClicked(Ref* sender);
	bool doubleDownButtonClicked(Ref* sender);
	bool hitButtonClicked(Ref* sender);
	bool standButtonClicked(Ref* sender);

	void disableAllChoiceButton();

	bool logOut(Ref* pSender);

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
	Dealer* _dealer;
	Hand*	_dealerHand = nullptr;


	COMMON::DealerInfo _dealerInfo;
	COMMON::UserInfo _userInfo[4];
	int				_userSlotNum;

	BetSlider*		_betSlider = nullptr;
	Menu*			_betButton = nullptr;

	Menu*			_choiceButton = nullptr;
	MenuItemLabel*	_itemSplit = nullptr;
	MenuItemLabel*	_itemDoubleDown = nullptr;
	MenuItemLabel*	_itemHit = nullptr;
	MenuItemLabel*	_itemStand = nullptr;
};