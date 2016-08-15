#include "pch.h"
#include "ui\UITextField.h"
#include "network\HttpRequest.h"
#include "network\HttpClient.h"
#include <thread>

#include "..\..\Common\ErrorCode.h"
#include "..\..\Common\Packet.h"
#include "ResourceInfo.h"
#include "LoginScene.h"
#include "NetworkManager.h"
#include "ClientLogger.h"

#pragma execution_character_set("UTF-8")

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

	initLayout();

	return true;
}

void LoginScene::menuCloseCallback(cocos2d::Ref* pSender)
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

std::wstring getStringFromFilename(std::string filename)
{
	std::wstring s;
	FileUtils::getInstance()->getContents(filename, &s);
	
	// truncated
	s.resize(wcslen(s.data()));

	return s;
}
void LoginScene::menuStartCallback(cocos2d::Ref* pSender)
{
 	auto name = _nameField->getString();
 	auto pass = _passField->getString();
 
 	if (name == "")
 		MessageBoxW(nullptr,L"�̸��� ��!", L"������",MB_OK);
 	if (pass == "")
 		MessageBoxW(nullptr, L"����� ��!", L"������", MB_OK);
 	auto request = new network::HttpRequest();
 	auto address = "http://10.73.43.23:8258/Request/Login";
 	request->setUrl(address);
 	request->setRequestType(network::HttpRequest::Type::POST);
 	request->setResponseCallback(CC_CALLBACK_2(LoginScene::startResponseCallback, this));
 	request->setHeaders({"Content-Type:application/json"});
 	std::string postData = "{\"UserID\" : \"" + name + "\",\"PW\" : \""
 		+ pass + "\"}";
 	request->setRequestData(postData.c_str(), postData.length());
 	network::HttpClient::getInstance()->send(request);
 	request->release();
}

/*
RES_LOGIN
{
��Result��: 0,
��AuthToken��: ��fExs6b7vrUp53bwZB5qtQ��, // 20�ڸ� ���ڿ�
��Pokemon��: 18,
��Channel��:
{
��Name��: ��ä�� ���ڽ���,
��Color��: ��F226DA��,
��IP��: ��127.0.0.1��,
��Port��: ��123��,
��MaxNum��: 100, // �ִ� ������ ��
��CurNum��: 0, // ���� ������ ��
��IsOpen��: true // �ʿ� ���� �� ����.
}
}
*/
void LoginScene::startResponseCallback(network::HttpClient* sender, network::HttpResponse* response)
{
	// �޾ƿ� ������ ���ڿ��� ��ȯ
	auto resData = response->getResponseData();
	auto resString = std::string("");
	for (auto& i : *resData)
		resString += i;

	if (resString == "")
	{
		ClientLogger::logThreadSafe("�α��� ������ �������� �ʽ��ϴ�!");
		ClientLogger::msgBox(L"�α��� ������ �������� �ʽ��ϴ�!",true);
		return;
	}
	// Json �Ľ�
	Json::Value loginRes;
	_reader.parse(resString, loginRes);
	auto result = (COMMON::RESULT_LOGIN)loginRes.get("Result", "failed").asInt();
	auto authToken = loginRes.get("AuthToken", "failed").asString();
	auto pokeNum = loginRes.get("Pokemon", -1).asInt();
	DEF::ChannelInfo channel;
	{
		auto channelValue = loginRes.get("Channel", "failed");
		channel.name = channelValue.get("Name", "failed").asString();
		channel.address = channelValue.get("IP", nullptr).asString();
		channel.port = channelValue.get("Port", nullptr).asInt();
		channel.maxUser = channelValue.get("MaxNum", nullptr).asInt();
		channel.currentUser = channelValue.get("CurNum", nullptr).asInt();
		{
			auto rgb = channelValue.get("Rgb", nullptr);
			channel.color.r = rgb.get("r", -1).asInt();
			channel.color.g = rgb.get("g", -1).asInt();
			channel.color.b = rgb.get("b", -1).asInt();
		}
	}
	auto bindFunc = CC_CALLBACK_0(NetworkManager::connectTcp, NetworkManager::getInstance(), channel.address, channel.port);
	auto newThread = std::thread(bindFunc);
	
}

void LoginScene::initLayout()
{
	// ���
	auto bg = Sprite::createWithSpriteFrameName(FILENAME::SPRITE::LOGIN_BG);
	bg->setAnchorPoint(Vec2(0,0));
	bg->getTexture()->setAliasTexParameters();
	addChild(bg);

	// ������ ��ư
	auto leaveLabel = Label::createWithTTF("������", FILENAME::FONT::SOYANON, 40);
	leaveLabel->setColor(Color3B(201, 201, 201));
	auto leaveButton = MenuItemLabel::create(leaveLabel, CC_CALLBACK_1(LoginScene::menuCloseCallback, this));
	auto leaveMenu = Menu::create(leaveButton, nullptr);
	leaveMenu->setPosition(1199.f, 674.f);
	this->addChild(leaveMenu, DEF::Z_ORDER::UI);

	// ���� ��ư
	auto startLabel = Label::createWithTTF("����", FILENAME::FONT::SOYANON, 72);
	startLabel->setColor(Color3B::GREEN);
	auto startButton = MenuItemLabel::create(startLabel, CC_CALLBACK_1(LoginScene::menuStartCallback, this));
	auto startMenu = Menu::create(startButton, nullptr);
	startMenu->setAnchorPoint(Vec2(0, 0));
	startMenu->setPosition(850, 230);
	addChild(startMenu);

	// �̸�
	_nameField = ui::TextField::create("�г���", FILENAME::FONT::SOYANON, 48);
	_nameField->setAnchorPoint(Vec2(0, 0));
	_nameField->setPosition(Vec2(585, 282));
	_nameField->setCursorEnabled(true);
	_nameField->setMaxLength(5);
	_nameField->setMaxLengthEnabled(true);
	addChild(_nameField);

	// ��й�ȣ
	_passField = ui::TextField::create("****", FILENAME::FONT::SOYANON, 48);
	_passField->setAnchorPoint(Vec2(0, 0));
	_passField->setPosition(Vec2(585, 170));
	_passField->setPasswordEnabled(true);
	_passField->setPasswordStyleText("*");
	_passField->setCursorEnabled(true);
	addChild(_passField);
}
