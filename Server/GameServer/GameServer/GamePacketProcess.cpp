#include "stdafx.h"
#include "Common.h"
#include "PacketProcess.h"
#include "RoomManager.h"
#include "UserManager.h"

ERROR_CODE PacketProcess::GameBet(PacketInfo packetInfo)
{
	auto reqPkt = (PacketGameBetReq*)packetInfo.pRefData;
	
	auto& room = m_pRefRoomMgr->GetRoomBySessionIndex(packetInfo.SessionIndex);

	auto ret = room->ApplyBet(packetInfo.SessionIndex, reqPkt->_betMoneyReq);
	if (ret != ERROR_CODE::NONE)
		return ret;

	return ERROR_CODE::NONE;
}

ERROR_CODE PacketProcess::GameChoice(PacketInfo packetInfo)
{
	auto reqPkt = (PacketGameChoiceReq*)packetInfo.pRefData;
	
	auto& room = m_pRefRoomMgr->GetRoomBySessionIndex(packetInfo.SessionIndex);
	auto user = m_pRefUserMgr->GetUserBySessionId(packetInfo.SessionIndex);
	auto prevSeatNhand = std::make_tuple(user->GetCurSeat(), user->GetCurHand());
	
	auto ret = room->ApplyChoice(packetInfo.SessionIndex, reqPkt->_choice);
	if (ret != ERROR_CODE::NONE)
	{
		room->NotifyChangeTurn();
		return ret;
	}

	room->NotifyGameChoice(packetInfo.SessionIndex, reqPkt->_choice);

	// �ڵ尡 ���� �ڵ�� �÷��� �� ���� ��Ƽ�� ����.
	auto nextSeatNhand = room->GetNextPlayerSeatAndHand();
	if(std::get<0>(prevSeatNhand) != std::get<0>(nextSeatNhand) || std::get<1>(prevSeatNhand) != std::get<1>(nextSeatNhand))
		room->NotifyChangeTurn();
	
	return ERROR_CODE::NONE;
}

