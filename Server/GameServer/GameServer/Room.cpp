#include "stdafx.h"

#include "User.h"
#include "IOCPManager.h"
#include "PacketQueue.h"
#include "DBmanager.h"

#include "Room.h"
#include "Dealer.h"

void Room::Init(PacketQueue* sendPacketQue, DBmanager* pDBman)
{
	m_pSendPacketQue = sendPacketQue;
	m_pDBmanager = pDBman;

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

	//find user
	int targetPos = -1;
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
	
	--m_currentUserCount;
	if (m_currentUserCount == 0)
	{
		m_currentRoomState = ROOM_STATE::NONE;
		m_waitForRestart = 100; // �ʱ�ȭ��..
		m_dealer.Clear();
	}
	else if (pUser->GetGameState() == GAME_STATE::ACTIONING) //������ �������̾����� ���� ������� ����
	{
		//turn ������ �˸�.
		pUser->SetGameState(GAME_STATE::ACTION_DONE);
		Logger::GetInstance()->Logf(Logger::Level::INFO, L"User(%s)'s Current Hand state : %d", pUser->GetName().c_str(), (int)pUser->GetHand(0)._handState);
		//pUser->SetHandState(0, HandInfo::HandState::BURST);

		NotifyChangeTurn();
	}

	// ������ ����/�������� �������� ����� ��
	//pUser->LeaveRoom();

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
	pkt._countTime = ServerConfig::bettingTime;
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
	
	for (int i = 0; i < 7; ++i)
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
		Logger::GetInstance()->Log(Logger::Level::ERROR_NORMAL, L"���û��°� �ƴѵ�.. ������.", 20);
		return ERROR_CODE::ROOM_GAME_NOT_IN_PROPER_STATE;
	}

	// ������ ���� ������ ��..
	auto ret = user->ApplyBet(betMoney);
	if (ret != ERROR_CODE::NONE)
		return ret;
	// DB�� �� �����ؼ� ����.
	m_pDBmanager->SubmitUserDeltaMoney(user, -betMoney);

	// ǥ���� �� �ְ� ���� ������ ��Ƽ�Ѵ�.
	NotifyBetDone(sessionIndex, betMoney);

	// ������ ���� �������� �� ���� ������ ��� ���ÿϷ��� ���� ����
	bool isAllBetReadyFlag = true;
	for (auto& user : m_userList)
	{
		if (user == nullptr)
		{
			continue;
		}

		if (user->GetGameState() != GAME_STATE::BET_DONE)
		{
			isAllBetReadyFlag = false;
		}
	}
	if (isAllBetReadyFlag)
	{
		NotifyStartGame();
	}

	return ERROR_CODE::NONE;
}

User* Room::GetCurrentBettingUser()
{
	for (int i = 0; i < MAX_USERCOUNT_PER_ROOM; ++i)
	{
		User* user = m_userList[i];
		if (user == nullptr)
			continue;

		if (user->GetGameState() == GAME_STATE::ACTIONING)
		{				
			return user;
		}
	}

	return nullptr;
}

void Room::ForceNextTurn(int seat, int hand)
{
	if (m_userList[seat] == nullptr)
	{
		return;
	}

	m_userList[seat]->SetGameState(GAME_STATE::ACTION_DONE);
	m_userList[seat]->SetHandState(hand, COMMON::HandInfo::HandState::STAND);
}

void Room::ForceBetting()
{
	for (int i = 0; i < MAX_USERCOUNT_PER_ROOM; ++i)
	{
		if (m_userList[i] == nullptr)
			return;

		ApplyBet(m_userList[i]->GetSessionIndex(), ServerConfig::minBet);
	}
}

ERROR_CODE Room::ApplyChoice(int sessionIndex, ChoiceKind choice)
{
	auto user = GetUserBySessionIndex(sessionIndex);

	switch (choice)
	{
	case ChoiceKind::STAND :
	{
		user->SetHandState(user->GetCurHand(), COMMON::HandInfo::HandState::STAND);
		user->SwitchHandIfSplitExist();
	}
	break;

	case ChoiceKind::HIT:
	{
		user->SetHand(user->GetCurHand(), m_dealer.Draw());

		auto sum = user->GetCardSum(user->GetCurHand());

		if (std::get<0>(sum) > 21)
		{
			user->SetHandState(user->GetCurHand(), COMMON::HandInfo::HandState::BURST);
			user->SwitchHandIfSplitExist();
		}
		else if (std::get<0>(sum) == 21 || std::get<1>(sum) == 21)
		{
			user->SetHandState(user->GetCurHand(), COMMON::HandInfo::HandState::STAND);
			user->SwitchHandIfSplitExist();
		}
		else
		{
			//TODO:: 21���� ������ �ؾ��� ó���� ������?
		}
	}
	break;

	case ChoiceKind::DOUBLE_DOWN:
	{
		if (!user->DoubleDown())
			return ERROR_CODE::ROOM_GAME_NOT_ENOUGH_MONEY;

		// DB�� �� �� ���� ����.
		m_pDBmanager->SubmitUserDeltaMoney(user, -(user->GetBetMoney()));

		user->SetHand(user->GetCurHand(), m_dealer.Draw());

		auto sum = user->GetCardSum(user->GetCurHand());

		if (std::get<0>(sum) > 21)
		{
			user->SetHandState(user->GetCurHand(), COMMON::HandInfo::HandState::BURST);
		}
		else
		{
			user->SetHandState(user->GetCurHand(), COMMON::HandInfo::HandState::STAND);
		}
		//�ٷ� ���� ������.
		user->SwitchHandIfSplitExist();

	}
	break;

	case ChoiceKind::SPLIT:
	{
		if (user->IsSplit())
			return ERROR_CODE::ROOM_GAME_INVALID_PLAY_ALREADY_SPLIT;
		
		Logger::GetInstance()->Logf(Logger::Level::INFO, L"Splite! user:%s", user->GetName().c_str());
		user->Split();
	}
	break;
	
	default:
		return ERROR_CODE::ROOM_GAME_INVALID_CHOICE;
	};

	return ERROR_CODE::NONE;
}

std::tuple<int, int> Room::GetNextPlayerSeatAndHand()
{
	for (int i = 0; i < MAX_USERCOUNT_PER_ROOM; ++i)
	{
		User* user = m_userList[i];
		if (user == nullptr)
			continue;

		if (user->GetGameState() != GAME_STATE::ACTION_WAITING && user->GetGameState() != GAME_STATE::ACTIONING)
			continue;

		for (int hand = 0; hand < MAX_HAND; ++hand)
		{
			if (hand != user->GetCurHand())
				continue;

			if (user->GetHand(hand)._handState != COMMON::HandInfo::HandState::CURRENT)
				continue;
			
			user->SetGameState(GAME_STATE::ACTIONING);
			return std::make_tuple(i, hand);
		}
	}

	return std::tuple<int, int>(-1, -1);
}

void Room::NotifyStartGame()
{
	// 2�� �ڿ� (ī�� �����ִ°� ������) ù ��° �������� ���� ����
	m_currentRoomState = ROOM_STATE::HANDOUT;
	m_lastActionTime = duration_cast< milliseconds >(
		steady_clock::now().time_since_epoch()
		).count();

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
			pkt._startSlotNum = m_userList[i]->GetCurSeat();

		}

		m_userList[i]->SetGameState(GAME_STATE::ACTION_WAITING);
	}

	// ���¸� �� �ٲ����� ������ ī�带 �����ֱ� �����Ѵ�.
	m_dealer.Init(this);
	
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
	if (m_currentRoomState == ROOM_STATE::HANDOUT)
		m_currentRoomState = ROOM_STATE::INGAME;

	PacketGameChangeTurnNtf pkt;
	
	std::tuple<int, int> SeatNhand = GetNextPlayerSeatAndHand();
	
	if (std::get<0>(SeatNhand) == -1 || std::get<1>(SeatNhand) == -1)
	{
		EndOfGame();
		return;
	}

	pkt._slotNum = std::get<0>(SeatNhand);
	pkt._handNum = std::get<1>(SeatNhand);
	pkt._waitingTime = ServerConfig::waitingTime;
	
	m_lastActionTime = duration_cast< milliseconds >(
		steady_clock::now().time_since_epoch()
		).count();

	for (int i = 0; i < MAX_USERCOUNT_PER_ROOM; ++i)
	{
		if (m_userList[i] == nullptr) continue;
		
		// Res ����
		PacketInfo sendPacket;
		sendPacket.SessionIndex = m_userList[i]->GetSessionIndex();
		sendPacket.PacketId = PACKET_ID::GAME_CHANGE_TURN_NTF;
		sendPacket.pRefData = (char *)&pkt;
		sendPacket.PacketBodySize = sizeof(pkt);
		m_pSendPacketQue->PushBack(sendPacket);
	}
}

void Room::NotifyGameChoice(int sessionIndex, ChoiceKind choice)
{
	PacketGameChoiceNtf pkt;
	auto user = GetUserBySessionIndex(sessionIndex);

	pkt._slotNum = user->GetCurSeat();
	pkt._choice = choice;

	if (choice == ChoiceKind::HIT || choice == ChoiceKind::DOUBLE_DOWN)
	{
		pkt._recvCard = user->GetLastCard(user->GetCurHand());
	}

	pkt._handNum = user->GetCurHand();
	pkt._betMoney = user->GetBetMoney();
	pkt._currentMoney = user->GetTotalMoney();
	pkt._waitingTime = ServerConfig::waitingTime;

	for (int i = 0; i < MAX_USERCOUNT_PER_ROOM; ++i)
	{
		if (m_userList[i] == nullptr) continue;
		
		// Res ����
		PacketInfo sendPacket;
		sendPacket.SessionIndex = m_userList[i]->GetSessionIndex();
		sendPacket.PacketId = PACKET_ID::GAME_CHOICE_NTF;
		sendPacket.pRefData = (char *)&pkt;
		sendPacket.PacketBodySize = sizeof(pkt);
		m_pSendPacketQue->PushBack(sendPacket);
	}
}

void Room::EndOfGame()
{
	using namespace COMMON;
	
	PacketGameDealerResultNtf pkt;

	while (m_dealer.GetCardSum() < 17)
	{
		m_dealer.SetHand(m_dealer.Draw());
	}

	if (m_dealer.GetCardNum() == 7)
	{
		m_dealer.SetHandState(HandInfo::HandState::SEVENCARD);
	}
	else if (m_dealer.GetCardSum() > 21)
	{
		m_dealer.SetHandState(HandInfo::HandState::BURST);
	}
	else
	{
		m_dealer.SetHandState(HandInfo::HandState::STAND);
	}

	for (int i = 0; i < MAX_USERCOUNT_PER_ROOM; ++i)
	{
		auto user = m_userList[i];
		if (user == nullptr) continue;

		int earnMoney = 0; // ��� ��� �� ��

		for (int hand = 0; hand < MAX_HAND; ++hand)
		{
			if (user->GetCurHand() < hand) continue;
			if (user->GetHand(hand)._handState == HandInfo::HandState::CURRENT) continue;

			auto sum = user->GetCardSum(hand);

			// ���� ������ ����Ʈ ������ ������ ���� ����!
			if (user->GetHand(hand)._handState == HandInfo::HandState::BURST)
			{
				earnMoney += 0;
				Logger::GetInstance()->Logf(Logger::Level::INFO, L"%s:BURST, total EarnMoney:%d", user->GetName().c_str(), earnMoney);
			}

			// ������ �а� �� ���ٸ�.. ���� ����!
			else if (m_dealer.GetHand()._handState > user->GetHand(hand)._handState)
			{
				earnMoney += 0;
				Logger::GetInstance()->Logf(Logger::Level::INFO, L"%s is lower than Dealer.  total EarnMoney:%d", user->GetName().c_str(), earnMoney);
			}

			// ���� �а� �� ���ٸ�.. ���� ��!
			else if (m_dealer.GetHand()._handState < user->GetHand(hand)._handState)
			{
				int blackjack_bonus = 0;
				if (user->GetHand(hand)._handState == HandInfo::HandState::BLACKJACK)
				{
					// �ٵ� �����̸� 1.5�踦 ��!
					blackjack_bonus = user->GetBetMoney() * 0.5;
				}
				earnMoney += user->GetBetMoney() * 2 + blackjack_bonus;

				//����ٿ��̸� �ű⿡ �ι踦 ��!
				if (user->GetHand(hand)._isDoubledown == true)
				{
					earnMoney += user->GetBetMoney() * 2;
				}
				Logger::GetInstance()->Logf(Logger::Level::INFO, L"%s is Higher than Dealer.  total EarnMoney:%d", user->GetName().c_str(), earnMoney);
			}
			// �а� ������
			else
			{
				
				// ���ĵ��̸� �� ���� ������ �ƴϸ� ���ɷ�
				if (user->GetHand(hand)._handState == HandInfo::HandState::STAND)
				{

					//Ace�� ���� ��� �� ���� ���ڸ� ����
					int useSum = 0;
					if (std::get<0>(sum) == std::get<1>(sum))
						useSum = std::get<0>(sum);
					else if(std::get<1>(sum) > 21) // Ace�� 11�� ����ϸ� �ȵǴ� ���
						useSum = std::get<0>(sum);
					else 
						useSum = std::get<1>(sum);

					if (useSum > m_dealer.GetCardSum()) // �̰�����
					{
						earnMoney += user->GetBetMoney() * 2;
						if (user->GetHand(hand)._isDoubledown == true)
							earnMoney += user->GetBetMoney() * 2;

					}
					else if (useSum < m_dealer.GetCardSum()) // ������
					{
						earnMoney += 0;
					}
					else // �������
					{
						if (user->GetHand(hand)._isDoubledown == true)
							earnMoney += user->GetBetMoney() * 2;
						else
							earnMoney += user->GetBetMoney();
					}
					Logger::GetInstance()->Logf(Logger::Level::INFO, L"%s 's score : %d , dealer's score : %d , total EarnMoney:%d", user->GetName().c_str(), useSum, m_dealer.GetCardSum(), earnMoney);
				}
				else
				{
					if(user->GetHand(hand)._isDoubledown == true)
						earnMoney += user->GetBetMoney() * 2;
					else
						earnMoney += user->GetBetMoney();

					Logger::GetInstance()->Logf(Logger::Level::INFO, L"%s is Same than Dealer.  total EarnMoney:%d", user->GetName().c_str(), earnMoney);
				}
			}
		}

		Logger::GetInstance()->Logf(Logger::Level::INFO, L"%s : resut : curBetMoney:%d, total EarnMoney:%d", user->GetName().c_str(),user->GetBetMoney() ,earnMoney);
		m_pDBmanager->SubmitUserDeltaMoney(user, earnMoney);
		user->CalculateMoney(earnMoney);
		
		pkt._currentMoney[i] = m_userList[i]->GetCurMoney();
		pkt._earnMoney[i] = earnMoney;

		if (earnMoney > user->GetBetMoney())
			pkt._winYeobu[i] = COMMON::PacketGameDealerResultNtf::WIN_YEOBU::WIN;
		else if(earnMoney == user->GetBetMoney()) 
			pkt._winYeobu[i] = COMMON::PacketGameDealerResultNtf::WIN_YEOBU::PUSH;
		else if(earnMoney == 0)
			pkt._winYeobu[i] = COMMON::PacketGameDealerResultNtf::WIN_YEOBU::LOSE;
	}

	for (int i = 1; i < 7; ++i)
	{
		pkt._dealerResult._openedCardList[i - 1] = m_dealer.GetCard(i);
	}

	for (int i = 0; i < MAX_USERCOUNT_PER_ROOM; ++i)
	{
		if (m_userList[i] == nullptr) continue;		

		// Res ����
		PacketInfo sendPacket;
		sendPacket.SessionIndex = m_userList[i]->GetSessionIndex();
		sendPacket.PacketId = PACKET_ID::GAME_DEALER_RESULT_NTF;
		sendPacket.pRefData = (char *)&pkt;
		sendPacket.PacketBodySize = sizeof(pkt);
		m_pSendPacketQue->PushBack(sendPacket);
	}

	ResetForNextGame();
}

void Room::ResetForNextGame()
{
	// x�� �ڿ� ������ �غ�
	m_waitForRestart = m_dealer.GetCardNum() * 1.0 + 2;

	m_dealer.Clear();

	for (int i = 0; i < MAX_USERCOUNT_PER_ROOM; ++i)
	{
		if (m_userList[i] == nullptr ) continue;

		m_userList[i]->ResetForNextGame();
	}

	m_currentRoomState = ROOM_STATE::CALCULATE;

	m_lastActionTime = m_lastActionTime = duration_cast< milliseconds >(
		steady_clock::now().time_since_epoch()
		).count();
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
