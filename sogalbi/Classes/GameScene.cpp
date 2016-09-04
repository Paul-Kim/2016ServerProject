#include "pch.h"

#include "..\Common\common.h"
#include "SimpleAudioEngine.h"
#include "ResourceInfo.h"
#include "NetworkManager.h"
#include "Player.h"
#include "ClientLogger.h"
#include "BetSlider.h"
#include "SimpleAudioEngine.h"
#include "Dealer.h"

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

	// ��Ʈ��ũ �۾� ������
	auto scheduler = Director::getInstance()->getScheduler();
	scheduler->performFunctionInCocosThread(tempLambda);

	// Ÿ�̸� ����
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
	// ���ȭ��
	auto background = Sprite::createWithSpriteFrameName(FILENAME::SPRITE::TABLE_BG);
	background->setAnchorPoint(Vec2(0, 0));
	background->getTexture()->setAliasTexParameters();
	this->addChild(background,Z_ORDER::BACKGROUND);

	// ������ ��ư
	auto leaveLabel = Label::createWithTTF(u8"������", FILENAME::FONT::SOYANON, 40);
	leaveLabel->setColor(Color3B(201, 201, 201));
	auto leaveButton = MenuItemLabel::create(leaveLabel, CC_CALLBACK_1(GameScene::menuCloseCallback, this));
	auto leaveMenu = Menu::create(leaveButton, nullptr);
	leaveMenu->setPosition(1199.f, 674.f);
	this->addChild(leaveMenu,Z_ORDER::UI_TOP);

	// ���� ��
	auto deck = Sprite::createWithSpriteFrameName(FILENAME::SPRITE::DECK);
	deck->setPosition(900.f, 700.f);
	deck->setRotation(-75.f);
	this->addChild(deck,Z_ORDER::CARD_TOP);

	// �� ��ȣ
	auto roomText = std::string("Room No : ") + std::to_string(roomNum);
	auto roomNumLabel = Label::createWithTTF(roomText, FILENAME::FONT::SOYANON, 32);
	roomNumLabel->setColor(Color3B(255, 255, 0));
	roomNumLabel->setPosition(20.f, 660.f);
	roomNumLabel->setAnchorPoint(Vec2(0.f, 0.f));
	this->addChild(roomNumLabel,Z_ORDER::UI_TOP);

	// �÷��̾� ������ �Ͳ�
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

	// ���� �ڵ�
// 	_dealerHand = Hand::create();
// 	addChild(_dealerHand, Z_ORDER::CARD_TOP);
// 	_dealerHand->setPosition(getContentSize().width / 2 - 60, 600);
	_dealer = Dealer::create();
	addChild(_dealer, Z_ORDER::CARD_TOP);
	_dealer->setPosition(getContentSize().width / 2, 600);


	// ���� �����̴�
	_betSlider = BetSlider::create();
	if (_betSlider == nullptr)
		_betSlider = BetSlider::create();
	_betSlider->setVisible(false);
	_betSlider->setPosition(Vec2(550, 30));
	addChild(_betSlider, Z_ORDER::UI_TOP);

	// ���� ��ư
	auto betLabel = Label::createWithTTF("BET", FILENAME::FONT::SOYANON, 64);
	auto betButton = MenuItemLabel::create(betLabel, CC_CALLBACK_1(GameScene::betButtonClicked, this));
	_betButton = Menu::create(betButton, nullptr);
	_betButton->setPosition(1170, 35);
	_betButton->setVisible(false);
	addChild(_betButton);

	// ���� ��ư
	auto labelSplit = Label::createWithTTF("SPLIT", FILENAME::FONT::PIXEL, 32);
	auto labelDoubleDown = Label::createWithTTF("DOUBLE DOWN", FILENAME::FONT::PIXEL, 32);
	auto labelHit = Label::createWithTTF("HIT", FILENAME::FONT::PIXEL, 32);
	auto labelStand = Label::createWithTTF("STAND", FILENAME::FONT::PIXEL, 32);

	_itemSplit = MenuItemLabel::create(labelSplit, CC_CALLBACK_1(GameScene::splitButtonClicked, this));
	_itemDoubleDown = MenuItemLabel::create(labelDoubleDown, CC_CALLBACK_1(GameScene::doubleDownButtonClicked, this));
	_itemHit = MenuItemLabel::create(labelHit, CC_CALLBACK_1(GameScene::hitButtonClicked, this));
	_itemStand = MenuItemLabel::create(labelStand, CC_CALLBACK_1(GameScene::standButtonClicked, this));
	disableAllChoiceButton();
	_choiceButton = Menu::create(_itemSplit, _itemDoubleDown, _itemHit, _itemStand,nullptr);
	_choiceButton->setPositionY(30);
	_choiceButton->alignItemsHorizontallyWithPadding(60);
	_choiceButton->setEnabled(false);
	addChild(_choiceButton, Z_ORDER::UI_TOP);

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
	case COMMON::PACKET_ID::GAME_CHANGE_TURN_NTF:
		packetProcess_GameChangeTurnNtf(packetInfo);
		break;
	case COMMON::PACKET_ID::GAME_CHOICE_NTF:
		packetProcess_GameChoiceNtf(packetInfo);
		break;
	case COMMON::PACKET_ID::GAME_DEALER_RESULT_NTF:
		packetProcess_GameDealerResultNtf(packetInfo);
		break;
	default:
		ClientLogger::msgBox(L"�𸣴� ��Ŷ");
		break;
	}
}

void GameScene::packetProcess_RoomEnterUserListRes(COMMON::RecvPacketInfo packetInfo)
{
	using namespace COMMON;
	auto userList = (PacketRoomUserlistRes*)packetInfo.pRefData;
	
	auto isAlreadyPlaying = false;
	for (int i = 0; i < MAX_USERCOUNT_PER_ROOM; ++i)
	{
		_players[i]->setPlayerDataWithUserInfo(userList->_users[i]);
		if (userList->_users->_hands[0]._cardList[0]._shape != CardInfo::CardShape::EMPTY)
			isAlreadyPlaying = true;
	}
	_userSlotNum = userList->_slot;
	_players[_userSlotNum]->setAsPlayer();
	if (isAlreadyPlaying)
		CocosDenshion::SimpleAudioEngine::getInstance()->playBackgroundMusic(FILENAME::AUDIO::GAME_BATTLE_BGM.c_str(),true);
	else
		CocosDenshion::SimpleAudioEngine::getInstance()->playBackgroundMusic(FILENAME::AUDIO::GAME_READY_BGM.c_str(),true);
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

bool GameScene::betButtonClicked(Ref* sender)
{
	using namespace COMMON;
	auto packet = PacketGameBetReq{_betSlider->getCurrentBet()};
	
	NetworkManager::getInstance()->sendPacket(PACKET_ID::GAME_BET_REQ, sizeof(packet), (char*)&packet);
	_betButton->setVisible(false);
	_betSlider->setVisible(false);
	_choiceButton->setVisible(true);
	return true;
}

void GameScene::packetProcess_GameBetNtf(COMMON::RecvPacketInfo packetInfo)
{
	using namespace COMMON;
	auto packet = (PacketGameBetNtf*)packetInfo.pRefData;
	auto& betUser = _players[packet->_betSlot];
	if (packet->_betSlot == _userSlotNum)
		_betSlider->setVisible(false);

	betUser->setMoneyBet(packet->_betMoney, betUser->getMoneyWhole() - packet->_betMoney);
	betUser->setAlreadyBet(true);
	betUser->initCounter();
}

void GameScene::packetProcess_GameBetCounter(COMMON::RecvPacketInfo packetInfo)
{
	using namespace COMMON;
	auto packet = (PacketGameBetCounterNtf*)packetInfo.pRefData;
	
	if (CocosDenshion::SimpleAudioEngine::getInstance()->isBackgroundMusicPlaying() == false)
		CocosDenshion::SimpleAudioEngine::getInstance()->playBackgroundMusic(FILENAME::AUDIO::GAME_READY_BGM.c_str(), true);
	
	auto& time = packet->_countTime;
	for (auto& user : _players)
	{
		if (user->isActivated() && !user->isAlreadyBet())
		{
			user->setCounter(time);
		}
		user->_hand[0]->clear();
		user->_hand[1]->clear();
		user->clearValueLabel();
		user->hideBanner();
	}
	if (_betSlider->isVisible() == false)
	{
		_betSlider->setMinBet(packet->minBet);
		_betSlider->setMaxBet(_players[_userSlotNum]->getMoneyWhole());
		_betSlider->setVisible(true);
		_betButton->setVisible(true);
	}
	_choiceButton->setVisible(false);
	//_dealerHand->clear();
	_dealer->clear();
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
		player->_hand[0]->pushCard(cards[1],0.5f);
		player->setValueLabel(player->_hand[0]->getHandValue());
	}
	//_dealerHand->pushCard(packet->_dealerCard);
	_dealer->pushCard(packet->_dealerCard);
	CocosDenshion::SimpleAudioEngine::getInstance()->playBackgroundMusic(FILENAME::AUDIO::GAME_BATTLE_BGM.c_str(), true);

	for (auto& player : _players)
	{
		if (player->_hand[0]->getHandValue().first == 21)
		{
			player->showBanner(Player::BannerKind::BLACK_JACK);
			CocosDenshion::SimpleAudioEngine::getInstance()->playEffect(FILENAME::AUDIO::STAND.c_str());
		}
	}

}

bool GameScene::splitButtonClicked(Ref* sender)
{
	using namespace COMMON;
	auto packet = PacketGameChoiceReq{};
	packet._choice = ChoiceKind::SPLIT;
	NetworkManager::getInstance()->sendPacket(PACKET_ID::GAME_CHOICE_REQ, sizeof(packet), (char*)&packet);

	disableAllChoiceButton();

	return true;
}

bool GameScene::doubleDownButtonClicked(Ref* sender)
{
	using namespace COMMON;
	auto packet = PacketGameChoiceReq{};
	packet._choice = ChoiceKind::DOUBLE_DOWN;
	NetworkManager::getInstance()->sendPacket(PACKET_ID::GAME_CHOICE_REQ, sizeof(packet), (char*)&packet);

	disableAllChoiceButton();

	return true;
}

bool GameScene::hitButtonClicked(Ref* sender)
{
	using namespace COMMON;
	auto packet = PacketGameChoiceReq{};
	packet._choice = ChoiceKind::HIT;
	NetworkManager::getInstance()->sendPacket(PACKET_ID::GAME_CHOICE_REQ, sizeof(packet), (char*)&packet);

	disableAllChoiceButton();

	return true;
}

bool GameScene::standButtonClicked(Ref* sender)
{
	using namespace COMMON;
	auto packet = PacketGameChoiceReq{};
	packet._choice = ChoiceKind::STAND;
	NetworkManager::getInstance()->sendPacket(PACKET_ID::GAME_CHOICE_REQ, sizeof(packet), (char*)&packet);

	disableAllChoiceButton();

	return true;
}

void GameScene::packetProcess_GameChangeTurnNtf(COMMON::RecvPacketInfo packetInfo)
{
	using namespace COMMON;
	auto packet = (PacketGameChangeTurnNtf*)packetInfo.pRefData;
	for (auto player : _players)
	{
		player->initCounter();
	}
	_players[packet->_slotNum]->setCounter(packet->_waitingTime);

	auto& player = _players[_userSlotNum];
	
	auto& hand = player->_hand[packet->_handNum];

	// �ش� hand�� enable �������
	hand->setVisible(true);
	player->_hand[(packet->_handNum + 1) % 2]->setVisible(false);


	if (packet->_slotNum == _userSlotNum)
	{
		auto& firstCard = hand->_cardInfos[0];
		auto& secondCard = hand->_cardInfos[1];

		_choiceButton->setEnabled(true);

		// split ��ư Ȱ��ȭ ����
		// ���ڰ� ������
		if (firstCard._number == secondCard._number)
		{
			// ���� ������
			if (player->getMoneyBet() <= player->getMoneyWhole())
			{
				_itemSplit->setEnabled(true);
			}
		}
		// double down ��ư Ȱ��ȭ ����
		if (player->getMoneyBet() <= player->getMoneyWhole())
		{
			_itemDoubleDown->setEnabled(true);
		}
		// hit ��ư Ȱ��ȭ ����
		_itemHit->setEnabled(true);
		// stand ��ư Ȱ��ȭ ����
		_itemStand->setEnabled(true);
	}
	else
		disableAllChoiceButton();
	// ��������
	auto flashEffect = Blink::create(0.5f, 2);
	_players[packet->_slotNum]->runAction(flashEffect);
	
}

void GameScene::packetProcess_GameChoiceNtf(COMMON::RecvPacketInfo packetInfo)
{
	using namespace COMMON;
	auto packet = (PacketGameChoiceNtf*)packetInfo.pRefData;
	auto& player = _players[packet->_slotNum];
	if(packet->_recvCard._shape != CardInfo::CardShape::EMPTY)
		player->_hand[packet->_handNum]->pushCard(packet->_recvCard);
	player->setMoneyBet(packet->_betMoney, packet->_currentMoney);
	player->setCounter(packet->_waitingTime);
	player->setValueLabel(player->_hand[packet->_handNum]->getHandValue());
	// �ڱ� �ڽ��̸�
	if (packet->_slotNum == _userSlotNum)
	{
		_itemHit->setEnabled(true);
		_itemStand->setEnabled(true);
	}
	else
	{
		disableAllChoiceButton();
	}

	// ����Ʈ �Ѹ���, ���� ���
	auto soundName = std::string{};
	switch (packet->_choice)
	{
	case ChoiceKind::HIT:
		player->showEffect(Player::EffectKind::HIT);
		soundName = FILENAME::AUDIO::HIT;
		break;
	case ChoiceKind::STAND:
		player->showEffect(Player::EffectKind::STAND);
		player->showBanner(Player::BannerKind::STAND);
		soundName = FILENAME::AUDIO::STAND;
		break;
	case ChoiceKind::SPLIT:
		player->showEffect(Player::EffectKind::SPLIT);
		player->_hand[1]->pushCard(player->_hand[0]->getCard(1)); 
		player->_hand[0]->popCard();
		break;
	case ChoiceKind::DOUBLE_DOWN:
		player->showEffect(Player::EffectKind::DOUBLE_DOWN);
		soundName = FILENAME::AUDIO::DOUBLE_DOWN;
		break;
	default:
		break;
	}
	auto value = player->_hand[packet->_handNum]->getHandValue();
	if (value.first > 21)
	{
		soundName = FILENAME::AUDIO::BURST;
		player->showBanner(Player::BannerKind::BURST);
		//player->_hand[packet->_handNum]->Die(.2f);
	}
	if (value.first == 21 || value.second == 21)
		player->showBanner(Player::BannerKind::STAND);

	if (soundName != "")
		CocosDenshion::SimpleAudioEngine::getInstance()->playEffect(soundName.c_str());
}

void GameScene::packetProcess_GameDealerResultNtf(COMMON::RecvPacketInfo packetInfo)
{
	auto packet = (PacketGameDealerResultNtf*)packetInfo.pRefData;

	// ��ư ���� ��Ȱ��ȭ
	disableAllChoiceButton();

	// ī���� ����
	for (auto& player : _players)
	{
		player->initCounter();
		player->setAlreadyBet(false);
	}
// 	// ������ ���� �� ī���
 	auto& cardList = packet->_dealerResult._openedCardList;
 	float waitingTime = 0.f;
// 	for (auto& card : cardList)
// 	{
// 		if (card._shape == CardInfo::CardShape::EMPTY)
// 			break;
// 		_dealerHand->pushCard(card, waitingTime);
// 		
// 		waitingTime += 1.f;
// 	}
// 
// 	//���� Burst�̸� ��Ӱ�
// 	if (_dealerHand->getHandValue().first > 21)
// 		_dealerHand->Die(waitingTime - .6f);

	for (auto& card : cardList)
	{
		if (card._shape == CardInfo::CardShape::EMPTY)
			break;
		waitingTime += 1.f;
	}
	_dealer->playDealerResultAnimation(packet->_dealerResult);

	// �� ��� �˷���
	for (int i = 0; i < 5; ++i)
	{
		if(_players[i]->_hand[0]->_cardInfos[0]._shape == CardInfo::CardShape::EMPTY)
			continue;
		auto callFunc = CallFunc::create(CC_CALLBACK_0(Player::setMoneyBet,_players[i],0,packet->_currentMoney[i]));
		_players[i]->runAction(Sequence::create(DelayTime::create(waitingTime), callFunc, nullptr));
	}

	auto winSound = std::string{};

	if (packet->_winYeobu[_userSlotNum] == PacketGameDealerResultNtf::WIN_YEOBU::WIN)
		winSound = FILENAME::AUDIO::RESULT_WIN;
	else if (packet->_winYeobu[_userSlotNum] == PacketGameDealerResultNtf::WIN_YEOBU::LOSE)
		winSound = FILENAME::AUDIO::RESULT_LOSE;
	auto winSoundFunc = CallFunc::create([winSound]() {
		CocosDenshion::SimpleAudioEngine::getInstance()->playEffect(winSound.c_str());
	}
	);
	runAction(Sequence::createWithTwoActions(DelayTime::create(waitingTime), winSoundFunc));


	// ���� ����?
	CocosDenshion::SimpleAudioEngine::getInstance()->stopBackgroundMusic(false);
}

void GameScene::disableAllChoiceButton()
{
	_itemSplit->setEnabled(false);
	_itemDoubleDown->setEnabled(false);
	_itemHit->setEnabled(false);
	_itemStand->setEnabled(false);
}
