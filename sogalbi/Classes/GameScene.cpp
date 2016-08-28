#include "pch.h"

#include "..\Common\common.h"
#include "SimpleAudioEngine.h"
#include "ResourceInfo.h"
#include "NetworkManager.h"
#include "Player.h"
#include "ClientLogger.h"
#include "BetSlider.h"

#include "GameScene.h"


Scene* GameScene::createScene(int roomNum)
{
	// 'scene' is an autorelease object
	auto scene = Scene::create();

	// 'layer' is an autorelease object
	auto layer = GameScene::create(roomNum);

	// add layer as a child to scene
	scene->addChild(layer);

	// return the scene
	return scene;
}

bool GameScene::init(int roomNum)
{
	using namespace COMMON;
	if (!Layer::init())
	{
		return false;
	}

	auto tempLambda = [=]() {
		initLayout(roomNum);
		NetworkManager::getInstance()->setRecvCallback(CC_CALLBACK_3(GameScene::recvPacketProcess, this));
		NetworkManager::getInstance()->sendPacket(PACKET_ID::ROOM_ENTER_USER_LIST_REQ, 0, nullptr);
	};

	// 네트워크 작업 스레딩
	auto scheduler = Director::getInstance()->getScheduler();
	scheduler->performFunctionInCocosThread(tempLambda);

	// 타이머 고고씽
	this->scheduleUpdate();

	return true;
}


void GameScene::menuCloseCallback(Ref* pSender)
{
	//Close the cocos2d-x game scene and quit the application
	Director::getInstance()->end();

#if (CC_TARGET_PLATFORM == CC_PLATFORM_IOS)
	exit(0);
#endif

	/*To navigate back to native iOS screen(if present) without quitting the application  ,do not use Director::getInstance()->end() and exit(0) as given above,instead trigger a custom event created in RootViewController.mm as below*/

	//EventCustom customEndEvent("game_scene_close_event");
	//_eventDispatcher->dispatchEvent(&customEndEvent);


}


void GameScene::initLayout(int roomNum)
{
	// 배경화면
	auto background = Sprite::createWithSpriteFrameName(FILENAME::SPRITE::TABLE_BG);
	background->setAnchorPoint(Vec2(0, 0));
	background->getTexture()->setAliasTexParameters();
	this->addChild(background,Z_ORDER::BACKGROUND);

	// 나가기 버튼
	auto leaveLabel = Label::createWithTTF(u8"나가기", FILENAME::FONT::SOYANON, 40);
	leaveLabel->setColor(Color3B(201, 201, 201));
	auto leaveButton = MenuItemLabel::create(leaveLabel, CC_CALLBACK_1(GameScene::menuCloseCallback, this));
	auto leaveMenu = Menu::create(leaveButton, nullptr);
	leaveMenu->setPosition(1199.f, 674.f);
	this->addChild(leaveMenu,Z_ORDER::UI_TOP);

	// 딜러 덱
	auto deck = Sprite::createWithSpriteFrameName(FILENAME::SPRITE::DECK);
	deck->setPosition(900.f, 700.f);
	deck->setRotation(-75.f);
	this->addChild(deck,Z_ORDER::CARD_TOP);

	// 방 번호
	auto roomText = std::string("Room No : ") + std::to_string(roomNum);
	auto roomNumLabel = Label::createWithTTF(roomText, FILENAME::FONT::SOYANON, 32);
	roomNumLabel->setColor(Color3B(255, 255, 0));
	roomNumLabel->setPosition(20.f, 660.f);
	roomNumLabel->setAnchorPoint(Vec2(0.f, 0.f));
	this->addChild(roomNumLabel,Z_ORDER::UI_TOP);

	// 플레이어 네임택 와꾸
	for (auto& player : _players)
	{
		player = Player::create();
		addChild(player,NAMETAG_BG);
	}
	auto screenSize = getContentSize();
	_players[4]->setPosition(130, 260);
	_players[3]->setPosition((screenSize.width / 2 + 130) / 2, 190);
	_players[2]->setPosition(screenSize.width / 2, 120);
	_players[1]->setPosition(screenSize.width - _players[3]->getPosition().x, 190);
	_players[0]->setPosition(screenSize.width - 130, 260);

	// 베팅 슬라이더
	_betSlider = BetSlider::create();
	_betSlider->setVisible(false);
	_betSlider->setPosition(Vec2(550, 30));
	addChild(_betSlider, Z_ORDER::UI_TOP);
	// 베팅 버튼
	auto betLabel = Label::createWithTTF("BET", FILENAME::FONT::SOYANON, 64);
	auto betButton = MenuItemLabel::create(betLabel, CC_CALLBACK_1(GameScene::betButtonClicked, this));
	_betButton = Menu::create(betButton, nullptr);
	_betButton->setPosition(1170, 35);
	_betButton->setVisible(false);
	addChild(_betButton);

	// 선택 버튼
	auto labelSplit = Label::createWithTTF("SPLIT", FILENAME::FONT::PIXEL, 32);
	auto labelDoubleDown = Label::createWithTTF("SPLIT", FILENAME::FONT::PIXEL, 32);
	auto labelHit = Label::createWithTTF("SPLIT", FILENAME::FONT::PIXEL, 32);
	auto labelStand = Label::createWithTTF("SPLIT", FILENAME::FONT::PIXEL, 32);

	//auto itemSplit = 
}

GameScene* GameScene::create(int roomNum)
{
	GameScene* pRet = new(std::nothrow) GameScene();
	if (pRet && pRet->init(roomNum))
	{
		pRet->autorelease();
		return pRet;
	}
	else
	{
		delete pRet;
		pRet = nullptr;
		return nullptr;
	}
}

void GameScene::recvPacketProcess(COMMON::PACKET_ID packetId, short bodySize, char* bodyPos)
{
	auto packetInfo = COMMON::RecvPacketInfo{};
	packetInfo.PacketId = packetId;
	packetInfo.PacketBodySize = bodySize;
	packetInfo.pRefData = bodyPos;
	switch (packetId)
	{
	case COMMON::PACKET_ID::ROOM_ENTER_USER_LIST_RES:
		packetProcess_RoomEnterUserListRes(packetInfo);
		break;
	case COMMON::PACKET_ID::ROOM_ENTER_USER_NTF:
		packetProcess_RoomEnterUserNtf(packetInfo);
		break;
	case COMMON::PACKET_ID::ROOM_LEAVE_USER_NTF:
		packetProcess_RoomLeaveUserNtf(packetInfo);
		break;
	case COMMON::PACKET_ID::GAME_BET_COUNTER_NTF:
		packetProcess_GameBetCounter(packetInfo);
		break;
	case COMMON::PACKET_ID::GAME_BET_NTF:
		packetProcess_GameBetNtf(packetInfo);
		break;
	case COMMON::PACKET_ID::GAME_START_NTF:
		packetProcess_GameStartNtf(packetInfo);
		break;
	default:
		ClientLogger::msgBox(L"모르는 패킷");
		break;
	}
}

void GameScene::packetProcess_RoomEnterUserListRes(COMMON::RecvPacketInfo packetInfo)
{
	using namespace COMMON;
	auto userList = (PacketRoomUserlistRes*)packetInfo.pRefData;
	for (int i = 0; i < MAX_USERCOUNT_PER_ROOM; ++i)
	{
		_players[i]->setPlayerDataWithUserInfo(userList->_users[i]);
	}
	_userSlotNum = userList->_slot;
	_players[_userSlotNum]->setAsPlayer();
}

void GameScene::packetProcess_RoomEnterUserNtf(COMMON::RecvPacketInfo packetInfo)
{
	using namespace COMMON;
	auto user = (PacketRoomEnterNtf*)packetInfo.pRefData;
	_players[user->_slotNum]->setPlayerDataWithUserInfo(user->_enterUser);
}

void GameScene::packetProcess_RoomLeaveUserNtf(COMMON::RecvPacketInfo packetInfo)
{
	using namespace COMMON;
	auto packet = (PacketRoomLeaveNtf*)packetInfo.pRefData;
	if (packet->_slotNum < 0 || packet->_slotNum >= MAX_USERCOUNT_PER_ROOM)
		return;
	_players[packet->_slotNum]->clear();
}

void GameScene::update(float dt)
{
}

void GameScene::packetProcess_GameBetCounter(COMMON::RecvPacketInfo packetInfo)
{
	using namespace COMMON;
	auto packet = (PacketGameBetCounterNtf*)packetInfo.pRefData;
	auto& time = packet->_countTime;
	for (auto& user : _players)
	{
		if (user->isActivated() && !user->isAlreadyBet())
		{
			user->setCounter(time);
		}
	}
	if (_betSlider->isVisible() == false)
	{
		_betSlider->setMinBet(packet->minBet);
		_betSlider->setMaxBet(_players[_userSlotNum]->getMoneyWhole());
		_betSlider->setVisible(true);
		_betButton->setVisible(true);
	}
}

bool GameScene::betButtonClicked(Ref* sender)
{
	using namespace COMMON;
	auto packet = PacketGameBetReq{_betSlider->getCurrentBet()};
	
	NetworkManager::getInstance()->sendPacket(PACKET_ID::GAME_BET_REQ, sizeof(packet), (char*)&packet);
	_betButton->setVisible(false);
	_betSlider->setVisible(false);
	return true;
}

void GameScene::packetProcess_GameBetNtf(COMMON::RecvPacketInfo packetInfo)
{
	using namespace COMMON;
	auto packet = (PacketGameBetNtf*)packetInfo.pRefData;
	auto& betUser = _players[packet->_betSlot];
	betUser->setMoneyBet(packet->_betMoney, betUser->getMoneyWhole() - packet->_betMoney);
	betUser->setAlreadyBet(true);
	betUser->initCounter();
}

void GameScene::packetProcess_GameStartNtf(COMMON::RecvPacketInfo packetInfo)
{
	using namespace COMMON;
	
	auto packet = (PacketGameStartNtf*)packetInfo.pRefData;

	for (int i = 0; i < 5; ++i)
	{
		auto& player = _players[i];
		if(player->isActivated() == false)
			continue;
		player->_hand[0]->setVisible(true);
		player->_hand[1]->setVisible(false);
		auto cards = packet->_handInfo[i]._cardList;
		player->_hand[0]->pushCard(cards[0]);
		player->_hand[0]->pushCard(cards[1]);
		player->setValueLabel(player->_hand[0]->getHandValue());
	}
}