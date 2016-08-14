#include "pch.h"
#include "LoginScene.h"
#include "ResourceInfo.h"
#include "ui\UITextField.h"
#include "network\HttpRequest.h"
#include "network\HttpClient.h"

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

void LoginScene::startResponseCallback(network::HttpClient* sender, network::HttpResponse* response)
{
	auto resData = response->getResponseData();
	auto resString = std::string("");
	for (auto& i : *resData)
		resString += i;

	MessageBox(resString.c_str(), "response");
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
	_passField = ui::TextField::create("asdfasdf", FILENAME::FONT::SOYANON, 48);
	_passField->setAnchorPoint(Vec2(0, 0));
	_passField->setPosition(Vec2(585, 170));
	_passField->setPasswordEnabled(true);
	_passField->setPasswordStyleText("*");
	_passField->setCursorEnabled(true);
	addChild(_passField);
}
