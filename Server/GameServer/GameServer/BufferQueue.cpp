#include "stdafx.h"
#include "NetworkSetting.h"
#include "BufferQueue.h"

BufferQueue* BufferQueue::_instance = nullptr;
BufferQueue* BufferQueue::GetInstance()
{
	if (_instance == nullptr)
		_instance = new BufferQueue();
	return _instance;
}

void BufferQueue::Init(NetworkSetting setting)
{
	// ����ڵ�
	if (_bufferPool.size() != 0)
		return;
	// ���� Ǯ ����
	for (unsigned i = 0; i < setting._maxBufferCount; ++i)
	{
		_bufferPool.emplace_back(new char[setting._maxBufferSize]);
	}
}

// ����� ���۸� Ǯ���� �ϳ� ���´�.
IOCPBuffer BufferQueue::GetBufferThreadSafe()
{
	// multiple readers on one container are safe!! �ų���
	while(true)
	{
		_mutex.lock();
		auto isEmpty = _bufferPool.empty();
		auto result = IOCPBuffer{};
		if (isEmpty == false)
		{
			result = _bufferPool.front();
			_bufferPool.pop_front();
		}
		_mutex.unlock();
		// pop ���� ������ ����
		if (isEmpty == false)
			return result;
	}
}

// ��� �Ϸ��� ���۸� ���� Ǯ�� �ٽ� �ִ´�
void BufferQueue::ReturnBufferThreadSafe(IOCPBuffer buffer)
{
	_mutex.lock();
	_bufferPool.emplace_back(buffer);
	_mutex.unlock();
}
