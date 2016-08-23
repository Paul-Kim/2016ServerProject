#include "stdafx.h"
#include <deque>
#include <mutex>

#include "PacketQueue.h"

// ���� ���� ����� �����ش�
RecvPacketInfo PacketQueue::ReadFront()
{
	if (this->IsEmpty())
	{
		auto nullPacket = RecvPacketInfo{};
		nullPacket.PacketId = PACKET_ID::NULL_PACKET;
		return nullPacket;
	}
	return _packetDeque.front();
}

// �� �Լ��� sync-safe ���� �ʴ�. ������������ ���ÿ� �θ��� ����� �������� ���Ѵ�.
void PacketQueue::PopFront()
{
	if (_packetDeque.empty())
		return;
	_mutex.lock();
	auto packet2Delete = _packetDeque.front();
	_packetDeque.pop_front();
	if (packet2Delete.pRefData != nullptr)
	{
		delete[] packet2Delete.pRefData;
		packet2Delete.pRefData = nullptr;
	}
	_mutex.unlock();
}

// �� �Լ��� sync-safe, thread-safe�ϴ�.
// �ƹ������� �� �ҷ��� ������. ���� ��� �뵵�� ��Ʈ��ũ ���̾�� �θ� �뵵
void PacketQueue::PushBack(RecvPacketInfo& recvPacket)
{
	// ��Ŷ ����
	auto copiedPacket = RecvPacketInfo{recvPacket};
	auto newBody = new char[copiedPacket.PacketBodySize];
	copiedPacket.pRefData = newBody;
	auto bodySize = recvPacket.PacketBodySize;
	memcpy_s(newBody, bodySize, recvPacket.pRefData, bodySize);
	
	//���� ���� ��Ŷ�� ť�� �־��ش�.
	_mutex.lock();
	_packetDeque.push_back(copiedPacket);
	_mutex.unlock();
}

bool PacketQueue::IsEmpty()
{
	return _packetDeque.empty();
}
