#include "pch.h"

#include "ResourceInfo.h"	

#include "Dealer.h"

bool Dealer::init()
{
	// ���� ��
	_pointLabel = Label::createWithTTF("", FILENAME::FONT::SOYANON, 24);
	_hand = Hand::create();

	return true;
}