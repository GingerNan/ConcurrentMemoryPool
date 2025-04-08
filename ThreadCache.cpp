#include "ThreadCache.h"

void* ThreadCache::Allocate(size_t size)
{
	assert(size <= MAX_BYTES);	// tc中单词只能申请不超过256KB的空间

	size_t alignSize = SizeClass::RoundUp(size);	// size对齐后的字节数
	size_t index = SizeClass::Index(size);			// size对应在哈希桶中的哪个桶

	if (!m_freeLists[index].Empty())
	{
		// 自由链表中不为空，可以直接从自由链表中获取空间
		return m_freeLists[index].Pop();
	}
	else
	{
		// 自由链表中为空，得到让 ThreadCache 向 CentralCache 申请空间
		return FetchFromCentralCache(index, alignSize);	// 至于为啥是这两个参数后面会讲到
	}
}

void ThreadCache::Deallocate(void* obj, size_t size)
{
	assert(obj); // 回收空间不能为空
	assert(size <= MAX_BYTES); // 回收空间大小不能超过256KB

	size_t index = SizeClass::Index(size);	// 找到size对应的自由链表
	m_freeLists[index].Push(obj);	// 用对应自由链表回收空间
}

void* ThreadCache::FetchFromCentralCache(size_t index, size_t alignSize)
{
	return nullptr;
}
