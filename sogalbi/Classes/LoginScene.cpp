#include "pch.h"
#include "ui\UITextField.h"
#include "network\HttpRequest.h"
#include "network\HttpClient.h"
#include "SimpleAudioEngine.h"

#include <thread>

#include "..\..\Common\ErrorCode.h"
#include "..\..\Common\Packet.h"
#include "ResourceInfo.h"
#include "LoginScene.h"
#include "NetworkManager.h"
#include "ClientLogger.h"
#include "GameScene.h"

#include "BetSlider.h"
#include "Hand.h"

cocos2d::Scene* LoginScene::createScene()
{
	auto scene = Scene::create();
	auto layer = LoginScene::create();
	scene->addChild(layer);

	return scene;
}

bool LoginScene::init()
{
	if (!Layer::init())
		return false;

	// ���̾ƿ�
	initLayout();

	auto recvCallback = std::bind(&LoginScene::recvPacketProcess, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	NetworkManager::getInstance()->setRecvCallback(recvCallback);

	// bgm
	CocosDenshion::SimpleAudioEngine::getInstance()->playBackgroundMusic(FILENAME::AUDIO::LOGIN_BGM.c_str(), true);

	return true;
}

void LoginScene::leaveButtonClicked(cocos2d::Ref* pSender)
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

void LoginScene::loginButtonClicked(cocos2d::Ref* pSender)
{
	CocosDenshion::SimpleAudioEngine::getInstance()->playEffect(FILENAME::AUDIO::LOGIN_BUTTON.c_str());
	_loginMenu->setEnabled(false);

	// �׽�Ʈ json �Է�
// 	loginResponseArrived(nullptr, nullptr);
// 	return;
	// �׽�Ʈ ��
	
	auto name = _nameField->getString();
	auto pass = _passField->getString();

	if (name == "")
		MessageBoxW(nullptr, L"�̸��� ��!", L"������", MB_OK);
	else if (pass == "")
		MessageBoxW(nullptr, L"����� ��!", L"������", MB_OK);
	else
	{
		auto request = new network::HttpRequest();
		auto address = "http://10.73.43.23:8258/Request/Login";
		request->setUrl(address);
		request->setRequestType(network::HttpRequest::Type::POST);
		request->setResponseCallback(CC_CALLBACK_2(LoginScene::loginResponseArrived, this));
		request->setHeaders({ "Content-Type:application/json" });
		std::string postData = "{\"UserID\" : \"" + name + "\",\"PW\" : \""
			+ pass + "\"}";
		request->setRequestData(postData.c_str(), postData.length());
		network::HttpClient::getInstance()->send(request);
		request->release();
		return;
	}
	// �̸��̳� ��� ��ģ ����̹Ƿ� �α��� ��ư �ٽ� ���� �� �ְ�
	_loginMenu->setEnabled(true);
}

void LoginScene::logoutButtonClicked(Ref* pSender)
{
	// �α׾ƿ���ư ��Ŭ ���ϰ�
	_logoutMenu->setEnabled(false);
	CocosDenshion::SimpleAudioEngine::getInstance()->playEffect(FILENAME::AUDIO::LOGOUT_BUTTON.c_str());

	popDownChannelsLayer();
	auto request = new network::HttpRequest();
	auto address = "http://10.73.43.23:8258/Request/Logout";
	request->setUrl(address);
	request->setRequestType(network::HttpRequest::Type::POST);
	request->setResponseCallback(CC_CALLBACK_2(LoginScene::logoutResponseArrived, this));
	request->setHeaders({ "Content-Type:application/json" });
	std::string postData = "{\"AuthToken\" : \"" + _authToken + "\"}";
	request->setRequestData(postData.c_str(), postData.length());
	network::HttpClient::getInstance()->send(request);
	request->release();
}

void LoginScene::loginResponseArrived(network::HttpClient* sender, network::HttpResponse* response)
{

	//auto resString = FileUtils::getInstance()->getStringFromFile("test.json");

	// �޾ƿ� ������ ���ڿ��� ��ȯ
	auto resData = response->getResponseData();
	auto resString = std::string("");
	for (auto& i : *resData)
 		resString += i;
	// result 10�϶� invalid password��
	if (resString == "")
	{
		ClientLogger::msgBox(L"�α��� ������ �������� �ʽ��ϴ�!", L"�α��� ����", false);
		_loginMenu->setEnabled(true); // �α��� ���������Ƿ� �ٽ� ���� �� �ְ�
		return;
	}
	// �޾ƿ� ���� �Ľ�
	auto channels = std::vector<DEF::ChannelInfo>();
	auto result = parseChannelInfo(resString, channels);
	if (result == COMMON::RESULT_LOGIN::ERR_INVALID_PASSWORD)
	{
		ClientLogger::msgBox(L"��� Ʋ����!", L"�α��� ����", false);
		_loginMenu->setEnabled(true);
		return;
	}
	else if (result == COMMON::RESULT_LOGIN::SUCCESS_CREATE_ACCOUNT || result == COMMON::RESULT_LOGIN::SUCCESS_LOGIN)
	{
		// �α��� ������
		CocosDenshion::SimpleAudioEngine::getInstance()->playBackgroundMusic(FILENAME::AUDIO::CHANNEL_SELECT_BGM.c_str(), true);
		popUpChannelsLayer(channels);
	}
	else
	{
		ClientLogger::msgBox(L"������ ������ �־��", L"�α��� ����", false);
	}

}

void LoginScene::logoutResponseArrived(network::HttpClient* sender, network::HttpResponse* response)
{
	// �α׾ƿ� ������
	// �α׾ƿ� ��ư �ٽ� ���� �� �ְ�
	_logoutMenu->setEnabled(true);
	_loginMenu->setEnabled(true);

	CocosDenshion::SimpleAudioEngine::getInstance()->playBackgroundMusic(FILENAME::AUDIO::LOGIN_BGM.c_str(),true);
}

void LoginScene::channelButtonClicked(DEF::ChannelInfo& targetChannel)
{
	// ä�ι�ư ������ Ŭ�� ���ϰ�
	_channelsMenu->setEnabled(false);
	log(u8"ä�� ���� �õ�");
	log(targetChannel.name.c_str());

	// ���� Ĩ���� �� �� �ִ� ä������ Ȯ��
	if (_currentChip < targetChannel.minBet || _currentChip > targetChannel.maxBet)
	{
		ClientLogger::msgBox(L"���� ���� Ĩ���δ� ������ �� �����ϴ�.", L"���� ����", false);
		// ä�ι�ư �ٽ� Ŭ���� �� �ְ�
		_channelsMenu->setEnabled(true);
		return;
	}
	
	auto newThread = std::thread(std::bind(&LoginScene::connectChannel, this, targetChannel.ip, targetChannel.port));
	
	newThread.detach();
}

void LoginScene::recvPacketProcess(COMMON::PACKET_ID packetId, short bodySize, char* bodyPos)
{
	auto packetInfo = COMMON::RecvPacketInfo{};
	packetInfo.PacketId = packetId;
	packetInfo.PacketBodySize = bodySize;
	packetInfo.pRefData = bodyPos;
	switch (packetId)
	{
	case COMMON::PACKET_ID::ROOM_ENTER_RES:
		packetProcess_RoomEnterRes(packetInfo);
		break;
	default:
		ClientLogger::msgBox(L"�𸣴� ��Ŷ");
		break;
	}
}

void LoginScene::popUpChannelsLayer(std::vector<DEF::ChannelInfo>& channelInfos)
{
	_channelsBg->setVisible(true);
	_loginMenu->setEnabled(false);
	_nameField->setEnabled(false);
	_passField->setEnabled(false);
	_logoutMenu->setVisible(true);

	_moneyLabel->setString(std::to_string(_currentChip));
	_moneyLabel->setVisible(true);
	_nameLabel->setString(_nameField->getString());
	_nameLabel->setVisible(true);
	int tmpPokeNum = (_pokeNum % MAX_NUMBER_OF_POKEMON_AVAILABLE) + 1;
	_pokeImg->setSpriteFrame(FILENAME::SPRITE::POKE_ARRAY[tmpPokeNum]);
	_pokeImg->setVisible(true);

	for (auto& channel : channelInfos)
	{
		auto channelNode = Node::create();
		
		auto buttonClicked = [=](Ref*) {
			auto copiedChannel = channel;
			channelButtonClicked(copiedChannel);
		};

		auto buttonLabel = Label::createWithTTF(channel.name, FILENAME::FONT::SOYANON, 42);
		buttonLabel->setColor(channel.rgb);
		auto buttonItem = MenuItemLabel::create(buttonLabel, buttonClicked);

		auto betString = std::string(u8"bet ");
		betString += std::to_string(channel.minBet); betString += u8"~"; betString += std::to_string(channel.maxBet);
		auto betLabel = Label::createWithTTF(betString,FILENAME::FONT::SOYANON, 32);
		if (_currentChip < channel.minBet || _currentChip > channel.maxBet)
			betLabel->setColor(Color3B(125, 0, 0)); // ���þ��� �ȸ´� ä���̸� ���������� �ٲ�
		betLabel->setPosition(buttonItem->getContentSize()/2 - Size(0, 40));

		buttonItem->addChild(betLabel);
		_channelsMenu->addChild(buttonItem);

		// ���Ŀ�
		buttonItem->setContentSize(betLabel->getContentSize());
	 }
	_channelsMenu->alignItemsHorizontallyWithPadding(100.f);
}


void LoginScene::popDownChannelsLayer()
{
	_channelsMenu->setEnabled(true);

	_channelsBg->setVisible(false);
	_logoutMenu->setVisible(false);
	_channelsMenu->removeAllChildren();
	
	_moneyLabel->setVisible(false);
	_nameLabel->setVisible(false);
	_pokeImg->setVisible(false);

	// _loginMenu->setEnabled(true); // �̰� �α׾ƿ� �����ÿ� �Ѵ�.
	_nameField->setEnabled(true);
	_passField->setEnabled(true);
}

void LoginScene::packetProcess_RoomEnterRes(COMMON::RecvPacketInfo packetInfo)
{
	using namespace COMMON;
	auto recvBody = (PacketRoomEnterRes*)packetInfo.pRefData;
	if (!recvBody->_errorCode == ERROR_CODE::NONE)
	{
		// TODO : ������ ó��
		return;
	}
	Director::getInstance()->getScheduler()->performFunctionInCocosThread([] {CocosDenshion::SimpleAudioEngine::getInstance()->stopBackgroundMusic(true); });
	
	auto gameScene = GameScene::createScene(recvBody->_roomNum);
	Director::getInstance()->pushScene(gameScene);
	return;
}

void LoginScene::initLayout()
{
	// ���
	auto bg = Sprite::createWithSpriteFrameName(FILENAME::SPRITE::LOGIN_BG);
	bg->setAnchorPoint(Vec2(0, 0));
	bg->getTexture()->setAliasTexParameters();
	addChild(bg,Z_ORDER::BACKGROUND);

	// ������ ��ư
	auto leaveLabel = Label::createWithTTF(u8"������", FILENAME::FONT::SOYANON, 40);
	leaveLabel->setColor(Color3B{ 201, 201, 201 });
	auto leaveButton = MenuItemLabel::create(leaveLabel, CC_CALLBACK_1(LoginScene::leaveButtonClicked, this));
	_leaveMenu = Menu::create(leaveButton, nullptr);
	_leaveMenu->setPosition(1199.f, 674.f);
	this->addChild(_leaveMenu, Z_ORDER::UI_ALWAYS_TOP);

	// �α��� ��ư
	auto startLabel = Label::createWithTTF(u8"�α���", FILENAME::FONT::SOYANON, 72);
	startLabel->setColor(Color3B::GREEN);
	auto startButton = MenuItemLabel::create(startLabel, CC_CALLBACK_1(LoginScene::loginButtonClicked, this));
	_loginMenu = Menu::create(startButton, nullptr);
	_loginMenu->setAnchorPoint(Vec2(0, 0));
	_loginMenu->setPosition(890, 254);
	addChild(_loginMenu, Z_ORDER::UI_LOGIN);

	// �α׾ƿ� ��ư
	auto logoutLabel = Label::createWithTTF(u8"�α׾ƿ�", FILENAME::FONT::SOYANON, 32);
	logoutLabel->setColor(Color3B{ 201,201,201 });
	auto logoutButton = MenuItemLabel::create(logoutLabel, CC_CALLBACK_1(LoginScene::logoutButtonClicked, this));
	_logoutMenu = Menu::create(logoutButton, nullptr);
	_logoutMenu->setAnchorPoint(Vec2(0, 0));
	_logoutMenu->setPosition(1050, 525);
	_logoutMenu->setVisible(false);
	addChild(_logoutMenu, Z_ORDER::UI_ALWAYS_TOP);

	// �̸�
	_nameField = ui::TextField::create(u8"�г���", FILENAME::FONT::SOYANON, 48);
	_nameField->setAnchorPoint(Vec2(0, 0));
	_nameField->setPosition(Vec2(544, 282));
	_nameField->setCursorEnabled(true);
	_nameField->setMaxLength(5);
	_nameField->setMaxLengthEnabled(true);
	addChild(_nameField, Z_ORDER::UI_LOGIN);

	// ��й�ȣ
	_passField = ui::TextField::create(u8"****", FILENAME::FONT::SOYANON, 48);
	_passField->setAnchorPoint(Vec2(0, 0));
	_passField->setPosition(Vec2(544, 170));
	_passField->setMaxLength(8);
	_passField->setMaxLengthEnabled(true);
	_passField->setPasswordEnabled(true);
	_passField->setPasswordStyleText("*");
	_passField->setCursorEnabled(true);
	addChild(_passField,Z_ORDER::UI_LOGIN);

	//��Ű ������ ������� �̵�
	auto eventListener = EventListenerKeyboard::create();
	eventListener->onKeyPressed = [&](EventKeyboard::KeyCode keyCode, Event* e) {
		if (_nameField->isFocused() && keyCode == EventKeyboard::KeyCode::KEY_TAB)
		{
			auto tmpNode = Widget::getCurrentFocusedWidget();
			_passField->setFocused(true);
			_passField->requestFocus();
			auto tmpNode2 = Widget::getCurrentFocusedWidget();
			//_passField->setCursorPosition(0);
			auto tmpNode3 = _nameField->findNextFocusedWidget(ui::Widget::FocusDirection::DOWN, _nameField);
		}
	};
	_eventDispatcher->addEventListenerWithSceneGraphPriority(eventListener, _nameField);

	// ä�� ����â ���
	_channelsBg = Sprite::createWithSpriteFrameName(FILENAME::SPRITE::CHANNEL_LIST_BG);
	_channelsBg->setAnchorPoint(Vec2::ZERO);
	_channelsBg->setPosition(Vec2::ZERO);
	_channelsBg->setVisible(false);
	addChild(_channelsBg,Z_ORDER::UI_CHANNELS_BG);

	// ä�� ���� �޴�
	_channelsMenu = Menu::create();
	addChild(_channelsMenu, Z_ORDER::UI_CHANNELS_ABOVE);

	// �� �߰�
	_moneyLabel = Label::createWithTTF("", FILENAME::FONT::SOYANON, 32);
	_moneyLabel->setColor(cocos2d::Color3B::GREEN);
	_moneyLabel->setAnchorPoint(Vec2(0, 0));
	_moneyLabel->setPosition(1088, 84);
	addChild(_moneyLabel, Z_ORDER::UI_ALWAYS_TOP);

	// ���ϸ� �߰�
	_pokeImg = Sprite::create();
	_pokeImg->setPosition(1040, 44);
	_pokeImg->getTexture()->setAliasTexParameters();
	addChild(_pokeImg, Z_ORDER::UI_ALWAYS_TOP);

	// �̸� �߰�
	_nameLabel = Label::createWithTTF("", FILENAME::FONT::SOYANON, 32);
	_nameLabel->setColor(cocos2d::Color3B::YELLOW);
	_nameLabel->setAnchorPoint(Vec2(0, 0));
	_nameLabel->setPosition(1088, 25);
	addChild(_nameLabel, Z_ORDER::UI_ALWAYS_TOP);
}

int LoginScene::parseChannelInfo(std::string& resLoginString, std::vector<DEF::ChannelInfo>& channelsVector)
{
	// Json �Ľ�
	Json::Value loginRes;
	_reader.parse(resLoginString, loginRes);
	auto result = (COMMON::RESULT_LOGIN)loginRes.get("Result", "failed").asInt();
	_authToken = loginRes.get("AuthToken", "failed").asString();
	_pokeNum = loginRes.get("Pokemon", -1).asInt();
	_currentChip = loginRes.get("Chip", 01).asInt();

	auto channelsValue = loginRes.get("Channels", "failed");
	for (auto& eachChannelValue : channelsValue)
	{
		auto eachChanneInfo = DEF::ChannelInfo();

		eachChanneInfo.name = eachChannelValue.get("Name", "failed").asString();
		eachChanneInfo.ip = eachChannelValue.get("IP", "failed").asString();
		eachChanneInfo.port = eachChannelValue.get("Port", -1).asInt();
		eachChanneInfo.minBet = eachChannelValue.get("MinBet", -1).asInt();
		eachChanneInfo.maxBet = eachChannelValue.get("MaxBet", -1).asInt();
		auto rgb = eachChannelValue.get("Rgb", "failed");
		eachChanneInfo.rgb.r = rgb.get("r", -1).asInt();
		eachChanneInfo.rgb.g = rgb.get("g", -1).asInt();
		eachChanneInfo.rgb.b = rgb.get("b", -1).asInt();

		channelsVector.emplace_back(std::move(eachChanneInfo));
	}
	return result;
}

void LoginScene::connectChannel(std::string ip, int port)
{
	if (!NetworkManager::getInstance()->connectTcp(ip, port))
	{
		// connect ���н�
		_channelsMenu->setEnabled(true); // ä�� ���� ��ư �ٽ� ���� �� �ְ�
		return;
	}
	// connect ������
	auto roomEnterPaket = COMMON::PacketRoomEnterReq();
	strcpy_s(roomEnterPaket._authToken, COMMON::AUTH_TOKEN_LEN+1, _authToken.c_str());
	auto result = NetworkManager::getInstance()->sendPacket(COMMON::ROOM_ENTER_REQ, sizeof(roomEnterPaket), (char*)&roomEnterPaket);
	if (result == false)
	{
		ClientLogger::logThreadSafe("send() ����");
		// ��Ÿ send ���н� ó��
		return;
	}
	// send ������
	// �� response ��ٸ���.
}