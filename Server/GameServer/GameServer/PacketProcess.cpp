#include "stdafx.h"
#include "PacketProcess.h"

#include "User.h"
#include "UserManager.h"

#include "Room.h"
#include "RoomManager.h"

#include "IOCPManager.h"



void PacketProcess::Init(UserManager * pUserMgr, RoomManager * pRoomMgr, PacketQueue* pRecvPacketQue, PacketQueue* pSendPacketQue)
{
	m_pRefUserMgr = pUserMgr;
	m_pRefRoomMgr = pRoomMgr;
	m_pRecvPacketQue = pRecvPacketQue;
	m_pSendPacketQue = pSendPacketQue;

	for (int i = 0; i < PACKET_ID::MAX; ++i)
	{
		PacketFuncArray[i] = nullptr;
	}

	PacketFuncArray[(int)ServerConfig::PACKET_ID::NTF_SYS_CLOSE_SESSION] = &PacketProcess::NtfSysCloseSesson;
	PacketFuncArray[(int)PACKET_ID::ROOM_ENTER_REQ] = &PacketProcess::RoomEnter;
	PacketFuncArray[(int)PACKET_ID::ROOM_LEAVE_REQ] = &PacketProcess::RoomLeave;
}

void PacketProcess::Process(PacketInfo packetInfo)
{
	auto packetId = packetInfo.PacketId;

	if (PacketFuncArray[packetId] == nullptr)
	{
		printf_s("�������� �ʴ� ID�� ��Ŷ�Դϴ�. ��ġ : %s \n", __FUNCTION__);
		return;
	}
	
	(this->*PacketFuncArray[packetId])(packetInfo);
}

ERROR_CODE PacketProcess::RoomEnter(PacketInfo packetInfo)
{
	auto reqPkt = (PacketRoomEnterReq*)packetInfo.pRefData;
	PacketRoomEnterRes resPkt;

	if (!m_pRefUserMgr->LoginUser(packetInfo.SessionIndex, std::string(reqPkt->_authToken)))
		return ERROR_CODE::ROOM_ENTER_CHANNEL_FULL;

	printf_s("����(%d)������ ���������� �� ��ω��� \n", packetInfo.SessionIndex);

	ERROR_CODE result = m_pRefRoomMgr->EnterUser(packetInfo.SessionIndex);
	if (result != ERROR_CODE::NONE)
		return result;

	printf_s("����(%d)�� ��(%d)�� ���� \n", packetInfo.SessionIndex, m_pRefUserMgr->GetUserBySessionIndex(packetInfo.SessionIndex)->GetCurRoomIdx());

	// �ٷ� ������ m_pRefUserMgr->GetUserBySessionIndex(packetInfo.SessionIndex)�� null���� Ȯ���ϹǷ� �� �� Ȯ���� �� ��..
	resPkt._roomNum = m_pRefUserMgr->GetUserBySessionIndex(packetInfo.SessionIndex)->GetCurRoomIdx();
	
	auto targetRoom = m_pRefRoomMgr->GetRoomById(resPkt._roomNum);

	// Res ����
	PacketInfo sendPacket;
	sendPacket.SessionIndex = packetInfo.SessionIndex;
	sendPacket.PacketId = PACKET_ID::ROOM_ENTER_RES;
	sendPacket.pRefData = (char *)&resPkt;
	sendPacket.PacketBodySize = sizeof(resPkt);
	m_pSendPacketQue->PushBack(sendPacket);
	
	// Notify
	targetRoom->NotifyEnterUserInfo(packetInfo.SessionIndex);

	return ERROR_CODE::NONE;
}

ERROR_CODE PacketProcess::RoomUserList(PacketInfo packetInfo)
{
	//[TODO] ...
	return ERROR_CODE();
}

ERROR_CODE PacketProcess::RoomLeave(PacketInfo packetInfo)
{
	auto reqPkt = (PacketRoomLeaveReq*)packetInfo.pRefData;
	PacketRoomLeaveRes resPkt;

	//�濡�� ������ �ڵ����� �α׾ƿ�
	if (!m_pRefUserMgr->LogoutUser(packetInfo.SessionIndex))
	{
		NtfSysCloseSesson(packetInfo);
		return ERROR_CODE::ROOM_LEAVE_NOT_MEMBER;
	}

	resPkt.SetErrCode(ERROR_CODE::NONE);
	
	PacketInfo sendPacketInfo;
	sendPacketInfo.PacketId = PACKET_ID::ROOM_LEAVE_RES;
	sendPacketInfo.SessionIndex = packetInfo.SessionIndex;
	sendPacketInfo.PacketBodySize = sizeof(resPkt);
	sendPacketInfo.pRefData = (char*)&resPkt;
	m_pSendPacketQue->PushBack(sendPacketInfo);

	return ERROR_CODE();
}

ERROR_CODE PacketProcess::RoomChat(PacketInfo packetInfo)
{
	//???? roomchat �츮 ���� ���ڰ� ���� ����?
	return ERROR_CODE();
}

ERROR_CODE PacketProcess::RoomChange(PacketInfo packetInfo)
{
	return ERROR_CODE();
}


ERROR_CODE PacketProcess::NtfSysCloseSesson(PacketInfo packetInfo)
{
	PacketInfo sendPacketInfo;
	sendPacketInfo.SessionIndex = packetInfo.SessionIndex;
	sendPacketInfo.PacketId = PACKET_ID::CLOSE_SESSION;
	sendPacketInfo.PacketBodySize = 0;
	sendPacketInfo.pRefData = nullptr;
	m_pSendPacketQue->PushBack(packetInfo);
	return ERROR_CODE();
}
