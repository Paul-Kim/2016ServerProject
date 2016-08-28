#include "stdafx.h"

#include "User.h"
#include "IOCPManager.h"
#include "PacketQueue.h"

#include "Room.h"
#include "Dealer.h"

void Room::Init(PacketQueue* sendPacketQue)
{
	m_pSendPacketQue = sendPacketQue;

	for (int i = 0; i < MAX_USERCOUNT_PER_ROOM; ++i)
	{
		m_userList[i] = nullptr;
	}
}

bool Room::EnterUser(User* user)
{
	int seat = GetAvailableSeat();
	if (seat == -1)
	{
		printf_s("��(%d)�� ���Դµ� �ڸ��� ����.. \n", m_id);
		return false;
	}
	user->EnterRoom(m_id);
	user->SetCurSeat(seat);
	m_userList[seat] = user;
	++m_currentUserCount;

	// ���� ���� ���� ��û �ð��� ����Ѵ�. ��Ƽ�� user list req ���� ���� ������.
	if (m_currentRoomState == ROOM_STATE::NONE || m_currentRoomState == ROOM_STATE::WAITING)
	{
		SetRoomStateToWaiting();
	}
	else
	{
		// ���� �̹� �������̸� ���� ������ waiting���� �ٲ��ش�.
		user->SetGameState(GAME_STATE::WAITING);
	}
	
	return true;
}

int Room::GetAvailableSeat()
{
	for (int i = 0; i < ServerConfig::MAX_USERCOUNT_PER_ROOM; ++i)
	{
		if (m_userList[i] == nullptr)
			return i;
	}
	return -1;
}

bool Room::CheckUserExist(int sessionIdx)
{
	for (int i = 0; i < MAX_USERCOUNT_PER_ROOM; ++i)
	{
		if (m_userList[i] == nullptr)
			continue;

		if (m_userList[i]->GetSessionIndex() == sessionIdx)
			return true;
	}

	return false;
}

COMMON::ERROR_CODE Room::LeaveRoom(User * pUser)
{
	// Notify
	NotifyLeaveUserInfo(pUser->GetSessionIndex());

	int targetPos = -1;

	//find user
	for (int i = 0; i < MAX_USERCOUNT_PER_ROOM ; ++i)
	{
		if (m_userList[i] == pUser)
		{
			targetPos = i;
			break;
		}
	}

	//Kick out User
	if (targetPos == -1)
		return COMMON::ERROR_CODE::ROOM_LEAVE_NOT_MEMBER;
	else
		m_userList[targetPos] = nullptr; 
	
	// ������ ����/�������� �������� ����� ��
	//pUser->LeaveRoom();

	--m_currentUserCount;
	if (m_currentUserCount == 0)
		m_currentRoomState = ROOM_STATE::NONE;

	WCHAR infoStr[100];
	wsprintf(infoStr, L"������ �α׾ƾƿ� �߽��ϴ�. RoomId:%d UserName:%s", m_id, pUser->GetName().c_str());
	Logger::GetInstance()->Log(Logger::INFO, infoStr, 100);

	pUser->Clear();

	return	COMMON::ERROR_CODE::NONE;
}

void Room::NotifyStartBettingTimer()
{
	// ���� ���°�  waiting�� �ƴϸ� ��Ƽ�� ������ �ȵ�
	if (m_currentRoomState != ROOM_STATE::WAITING)
		return;
	
	PacketGameBetCounterNtf pkt;
	pkt._countTime = 10;
	pkt.minBet = ServerConfig::minBet;
	pkt.maxBet = ServerConfig::maxBet;

	for (int i = 0; i < MAX_USERCOUNT_PER_ROOM; ++i)
	{
		if (m_userList[i] == nullptr) continue;

		// Res ����
		PacketInfo sendPacket;
		sendPacket.SessionIndex = m_userList[i]->GetSessionIndex();
		sendPacket.PacketId = PACKET_ID::GAME_BET_COUNTER_NTF;
		sendPacket.pRefData = (char *)&pkt;
		sendPacket.PacketBodySize = sizeof(pkt);
		m_pSendPacketQue->PushBack(sendPacket);
	}
}

void Room::NotifyBetDone(int sessionIndex, int betMoney)
{
	PacketGameBetNtf pkt;

	pkt._betMoney = betMoney;
	pkt._betSlot = GetUserSeatBySessionIndex(sessionIndex);

	for (int i = 0; i < MAX_USERCOUNT_PER_ROOM; ++i)
	{
		if (m_userList[i] == nullptr) continue;

		// Res ����
		PacketInfo sendPacket;
		sendPacket.SessionIndex = m_userList[i]->GetSessionIndex();
		sendPacket.PacketId = PACKET_ID::GAME_BET_NTF;
		sendPacket.pRefData = (char *)&pkt;
		sendPacket.PacketBodySize = sizeof(pkt);
		m_pSendPacketQue->PushBack(sendPacket);
	}
}

void Room::SetRoomStateToWaiting()
{
	m_currentRoomState = ROOM_STATE::WAITING;
	m_lastActionTime = duration_cast< milliseconds >(
		steady_clock::now().time_since_epoch()
		).count();

	// ������ ���� ������ �ؾ��ϹǷ� betting���� �ٲ��ش�.
	for (auto& user : m_userList)
	{
		if (user == nullptr)
			continue;

		user->SetGameState(GAME_STATE::BETTING);
	}
}

COMMON::DealerInfo Room::GetDealerInfo()
{
	COMMON::DealerInfo dInfo;
	
	for (int i = 0; i < 8; ++i)
	{
		dInfo._openedCardList[i] = m_dealer.GetCard(i);
	}

	return dInfo;
}

ERROR_CODE Room::ApplyBet(int sessionIndex, int betMoney)
{
	User* user = GetUserBySessionIndex(sessionIndex);

	// ���°� ������ �ƴѵ� ���� ������ �Ϸ� �ϴٴ�..
	if (user->GetGameState() != GAME_STATE::BETTING)
	{
		return ERROR_CODE::ROOM_GAME_NOT_IN_PROPER_STATE;
	}

	// ������ ���� ������ ��..
	auto ret = user->ApplyBet(betMoney);
	if (ret != ERROR_CODE::NONE)
		return ret;

	// ǥ���� �� �ְ� ���� ������ ��Ƽ�Ѵ�.
	NotifyBetDone(sessionIndex, betMoney);

	// ������ ���� �������� �� ���� ������ ��� ���ÿϷ��� ���� ����
	bool flag = true;
	for (auto& user : m_userList)
	{
		if (user == nullptr)
		{
			continue;
		}

		if (user->GetGameState() != GAME_STATE::BET_DONE &&
			user->GetGameState() != GAME_STATE::WAITING)
		{
			flag = false;
		}
	}
	if (flag)
	{
		NotifyStartGame();
	}

	return ERROR_CODE::NONE;
}

void Room::NotifyStartGame()
{
	m_currentRoomState = ROOM_STATE::INGAME;

	PacketGameStartNtf pkt;

	bool flag = true;
	for (int i=0; i<MAX_USERCOUNT_PER_ROOM; ++i)
	{
		if (m_userList[i] == nullptr)
			continue;
		else if (flag)
		{
			// ���� �� ������ �������� ���� �ְ� ������ġ��� ǥ���Ѵ�
			flag = false;
			m_userList[i]->SetGameState(GAME_STATE::ACTIONING);
			pkt._startSlotNum = m_userList[i]->GetCurSeat();
		}
		else
			m_userList[i]->SetGameState(GAME_STATE::ACTION_WAITING);

		// ���¸� �� �ٲ����� ������ ī�带 �����ֱ� �����Ѵ�.
		m_dealer.Init(this);
	}
	
	//��Ŷ ����
	pkt._turnCountTime = 10;
	pkt._dealerCard = m_dealer.GetCard(0);

	for (int i = 0; i < MAX_USERCOUNT_PER_ROOM; ++i)
	{
		if (m_userList[i] == nullptr) continue;

		pkt._handInfo[i] = m_userList[i]->GetHand(0);
	}

	//��Ŷ ����
	for (int i = 0; i < MAX_USERCOUNT_PER_ROOM; ++i)
	{
		if (m_userList[i] == nullptr) continue;

		// Res ����
		PacketInfo sendPacket;
		sendPacket.SessionIndex = m_userList[i]->GetSessionIndex();
		sendPacket.PacketId = PACKET_ID::GAME_START_NTF;
		sendPacket.pRefData = (char *)&pkt;
		sendPacket.PacketBodySize = sizeof(pkt);
		m_pSendPacketQue->PushBack(sendPacket);
	}
}

void Room::NotifyChangeTurn()
{
}

// sessionIndex = ���� ���� -> ���� ���������� ����
void Room::NotifyEnterUserInfo(int sessionIndex)
{
	auto enterUser = GetUserBySessionIndex(sessionIndex);

	PacketRoomEnterNtf pkt;
	pkt._enterUser = enterUser->GetUserInfo();
	pkt._slotNum = GetUserSeatBySessionIndex(sessionIndex);

	for (int i = 0; i < MAX_USERCOUNT_PER_ROOM; ++i)
	{
		if (m_userList[i] == nullptr) continue;

		if (m_userList[i]->CheckUserWithSessionIndex(sessionIndex))
			continue;

		// Res ����
		PacketInfo sendPacket;
		sendPacket.SessionIndex = m_userList[i]->GetSessionIndex();
		sendPacket.PacketId = PACKET_ID::ROOM_ENTER_USER_NTF;
		sendPacket.pRefData = (char *)&pkt;
		sendPacket.PacketBodySize = sizeof(pkt);
		m_pSendPacketQue->PushBack(sendPacket);
	}
}

void Room::NotifyLeaveUserInfo(int sessionIndex)
{
	auto leaveUser = GetUserBySessionIndex(sessionIndex);

	PacketRoomLeaveNtf pkt;
	pkt._slotNum = leaveUser->GetCurSeat();

	for (int i = 0; i < MAX_USERCOUNT_PER_ROOM; ++i)
	{
		if (m_userList[i] == nullptr) continue;

		if (m_userList[i]->CheckUserWithSessionIndex(sessionIndex))
			continue;

		// Res ����
		PacketInfo sendPacket;
		sendPacket.SessionIndex = m_userList[i]->GetSessionIndex();
		sendPacket.PacketId = PACKET_ID::ROOM_LEAVE_USER_NTF;
		sendPacket.pRefData = (char *)&pkt;
		sendPacket.PacketBodySize = sizeof(pkt);
		m_pSendPacketQue->PushBack(sendPacket);
	}
}

User * Room::GetUserBySessionIndex(int sessionIndex)
{
	for (int i = 0; i < MAX_USERCOUNT_PER_ROOM; ++i)
	{
		if (m_userList[i] != nullptr)
		{
			if (m_userList[i]->CheckUserWithSessionIndex(sessionIndex))
				return m_userList[i];
		}
	}

	return nullptr;
}

int Room::GetUserSeatBySessionIndex(int sessionIndex)
{
	for (int i = 0; i < MAX_USERCOUNT_PER_ROOM; ++i)
	{
		if (m_userList[i] != nullptr)
		{
			if (m_userList[i]->CheckUserWithSessionIndex(sessionIndex))
				return i;
		}
	}

	return -1;
}
