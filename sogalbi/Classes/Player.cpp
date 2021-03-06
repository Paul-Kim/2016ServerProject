#include "pch.h"
#include "..\Common\Common.h"
#include "Player.h"
#include "ResourceInfo.h"
#include "ClientLogger.h"

bool Player::init()
{
	_nameTag = Sprite::createWithSpriteFrameName(FILENAME::SPRITE::NAMETAG);
	_nameTagBack = Sprite::createWithSpriteFrameName(FILENAME::SPRITE::NAMETAG_BACK);

	auto counterSprite = Sprite::createWithSpriteFrameName(FILENAME::SPRITE::NAMETAG_BACK);
	counterSprite->setColor(Color3B::RED);
	_timer = ProgressTimer::create(counterSprite);
	_timer->setType(ProgressTimer::Type::RADIAL);
	_timer->setBarChangeRate(Vec2(1, 1));
	_timer->setMidpoint(Vec2(0.5, 0.5));


	_nameLabel = Label::createWithTTF("", FILENAME::FONT::SOYANON, 24);
	_pokemon = Sprite::create();

	_moneyLabelFront = Label::createWithTTF(u8"", FILENAME::FONT::SOYANON, 24);
	_moneyLabelBack = Label::createWithTTF(u8"", FILENAME::FONT::SOYANON, 24);
	
	addChild(_timer, 1);

	addChild(_nameTag,1);
	addChild(_nameTagBack,0);
	addChild(_nameLabel,2);
	addChild(_pokemon, 2);
	addChild(_moneyLabelFront, 2);
	addChild(_moneyLabelBack, 2);

	_moneyLabelFront->setAnchorPoint(Vec2(1, 1));
	_moneyLabelBack->setAnchorPoint(Vec2(0, 1));
	_moneyLabelFront->setPosition(25, 0);
	_moneyLabelBack->setPosition(25, 0);

	_nameLabel->setAnchorPoint(Vec2(0, 0));
	_nameLabel->setPosition(-45, 0);

	_pokemon->setPosition(-80.f, 0.f);

	// 핸드
	_hand[0] = Hand::create();
	_hand[1] = Hand::create();
	_hand[1]->setVisible(false);

	_hand[0]->setPosition(-50,70);
	_hand[1]->setPosition(-50,70);

	addChild(_hand[0], -1);
	addChild(_hand[1], -1);

	// 카드 밸류
	_valueLabel = Label::createWithTTF("", FILENAME::FONT::SOYANON, 48);
	addChild(_valueLabel, 3);
	_valueLabel->setColor(Color3B::YELLOW);
	_valueLabel->setPositionY(200);

	// 이펙트
	_effectSprite = Sprite::create();
	addChild(_effectSprite, 4);
	_effectSprite->setPosition(70, 200);

	// 배너
	_bannerSprite = Sprite::create();
	addChild(_bannerSprite,2);
	_bannerSprite->setPositionY(52);

	return true;
}

void Player::setPlayerDataWithUserInfo(COMMON::UserInfo userInfo)
{
	if (userInfo._name[0] == '\0')
		return;
	_isActivated = true;
	_nameLabel->setVisible(true);
	_moneyLabelBack->setVisible(true);
	_moneyLabelFront->setVisible(true);
	_pokemon->setVisible(true);

	setPoke(userInfo._pokeNum);
	
	char utfChar[128];
	ZeroMemory(utfChar, sizeof(utfChar));
	int nLen = WideCharToMultiByte(CP_UTF8, 0, userInfo._name, lstrlen(userInfo._name), nullptr, 0, nullptr, nullptr);
	WideCharToMultiByte(CP_UTF8, 0, userInfo._name, lstrlen(userInfo._name), utfChar, nLen, nullptr, nullptr);
	setNameLabel(utfChar);

	setMoneyBet(userInfo._betMoney, userInfo._totalMony);


	// 핸드부분
	int curHandNum = 0;
	if (userInfo._curHand == 0)
	{
		curHandNum = 0;
		_hand[0]->setVisible(true);
		_hand[1]->setVisible(false);
	}
	else
	{
		_hand[1]->setVisible(true);
		_hand[0]->setVisible(false);
	}
	for (int i = 0; i<2; ++i)
	{
		for (auto& card : userInfo._hands[i]._cardList)
		{
			if (card._shape == CardInfo::CardShape::EMPTY)
				break;
			_hand[i]->pushCard(card);
		}
	}
	setValueLabel(_hand[curHandNum]->getHandValue());
}

void Player::clear()
{
	_isActivated = false;
	_nameLabel->setVisible(false);
	_moneyLabelBack->setVisible(false);
	_moneyLabelFront->setVisible(false);
	_pokemon->setVisible(false);
	_timer->setPercentage(0);
	_timer->stopAllActions();

	_hand[0]->setVisible(true);
	_hand[1]->setVisible(false);
	_hand[0]->clear();
	_hand[1]->clear();

	_valueLabel->setString("");
}

void Player::setAsPlayer()
{
	_nameTagBack->setColor(Color3B::YELLOW);
	_nameLabel->setColor(Color3B::YELLOW);
	_moneyLabelFront->setColor(Color3B::GREEN);
}

void Player::setNameLabel(std::string name)
{
	_nameLabel->setString(name);
}

void Player::setPoke(std::string pokeFileName)
{
	_pokemon->setSpriteFrame(pokeFileName);
}

void Player::setPoke(int pokeNum)
{
	if (pokeNum == 0)
		return;
	pokeNum = pokeNum % MAX_NUMBER_OF_POKEMON_AVAILABLE + 1;
	_pokemon->setSpriteFrame(FILENAME::SPRITE::POKE_ARRAY[pokeNum]);
}

void Player::setMoneyBet(int bet,int whole)
{
	_betMoney = bet;
	_money = whole;
	_moneyLabelFront->setString(std::to_string(bet));
	_moneyLabelBack->setString(u8"/"+std::to_string(whole));
}

void Player::setCounter(float countTime)
{
	_timer->runAction(ProgressFromTo::create(countTime, 0, 100));
}

void Player::initCounter()
{
	_timer->stopAllActions();
	_timer->setPercentage(0.f);
}

void Player::setValueLabel(std::pair<int, int> values)
{
	auto valueString = std::string{};
	if (values.first == values.second)
		valueString = std::to_string(values.first);
	else
		valueString = std::to_string(values.first) + "/" + std::to_string(values.second);
	_valueLabel->setString(valueString);
}

void Player::showEffect(EffectKind effect)
{
	_effectSprite->setVisible(true);

	auto frameName = std::string{};
	switch (effect)
	{
	case Player::EffectKind::HIT:
		frameName = FILENAME::SPRITE::HIT;
		break;
	case Player::EffectKind::STAND:
		frameName = FILENAME::SPRITE::STAND;
		break;
	case Player::EffectKind::SPLIT:
		frameName = FILENAME::SPRITE::SPLIT;
		break;
	case Player::EffectKind::DOUBLE_DOWN:
		frameName = FILENAME::SPRITE::DOUBLE_DOWN;
		break;
	default:
		break;
	}
	auto spriteFrame = SpriteFrameCache::getInstance()->getSpriteFrameByName(frameName);

	_effectSprite->setSpriteFrame(spriteFrame);
	auto blink = Blink::create(0.5, 2);
	auto callFunc = CallFunc::create(CC_CALLBACK_0(Sprite::setVisible,_effectSprite,false));
	auto seq = Sequence::create(blink,DelayTime::create(0.5f), callFunc, nullptr);
	_effectSprite->runAction(seq);
}

void Player::showBanner(BannerKind banner)
{
	_bannerSprite->setVisible(true);

	auto frameName = std::string{};

	switch (banner)
	{
	case BannerKind::BLACK_JACK:
		frameName = FILENAME::SPRITE::BANNER_BLACKJACK;
		break;
	case BannerKind::STAND:
		frameName = FILENAME::SPRITE::BANNER_STAND;
		break;
	case BannerKind::BURST:
		frameName = FILENAME::SPRITE::BANNER_BURST;
		break;
	case BannerKind::WAITING:
		frameName = FILENAME::SPRITE::BANNER_WAITING;
		break;
	default:
		break;
	}
	auto frame = SpriteFrameCache::getInstance()->getSpriteFrameByName(frameName);
	_bannerSprite->setSpriteFrame(frame);
}
