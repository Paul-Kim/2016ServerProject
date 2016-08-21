#include "stdafx.h"
#include <deque>
#include <mutex>

#include "PacketQueue.h"

// �� �Լ��� sync-safe ���� �ʴ�. ������������ ���ÿ� �θ��� ����� �������� ���Ѵ�.
RecvPacketInfo PacketQueue::ReadFront()
{
	return _packetDeque.front();
}

// �� �Լ��� sync-safe ���� �ʴ�. ������������ ���ÿ� �θ��� ����� �������� ���Ѵ�.
void PacketQueue::PopFront()
{
	if (_packetDeque.empty())
		return;
	_mutex.lock();
	auto packet2Delete = _packetDeque.front();
	delete[] packet2Delete.pRefData;
	_mutex.unlock();
}

// �� �Լ��� sync-safe, thread-safe�ϴ�. �ƹ������� �� �ҷ��� ������. ���� ��� �뵵�� ��Ʈ��ũ ���̾�� �θ� �뵵
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