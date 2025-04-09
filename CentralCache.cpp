#include "CentralCache.h"

CentralCache CentralCache::m_sInst;	// CentralCache的饿汉对象

size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	// 获取size对应哪一个SpanList
	size_t index = SizeClass::Index(size);

	// 对cc中的SpanList操作时要加锁
	m_spanLists[index].m_mutex.lock();

	// 获取到一个管理空间非空时要加锁
	Span* span = GetOneSpan(m_spanLists[index], size);
	assert(span);	// 断言一下span不为空
	assert(span->_freeList);	// 断言一下span管理的空不能为空

	// 起初都是指向_freeList，让end不断往后走
	start = end = span->_freeList;
	size_t actualNum = 1; // 函数实际的返回值

	// 在end的next不为空的前提下，让end走batchNum - 1步
	size_t i = 0;
	while (i < batchNum - 1 && ObjNext(end) != nullptr)
	{
		end = ObjNext(end);
		++actualNum; // 记录end走过了多少步
		++i;	// 控制条件
	}

	// 将[start，end]返回给ThreadCache后，调整Span的_freeList
	span->_freeList = ObjNext(end);
	span->_usecount += actualNum; // 给tc分了多少就给useCount加多少
	// 返回一段空间，不要和原先Span的_freeList中的块相连
	ObjNext(end) = nullptr;

	m_spanLists[index].m_mutex.unlock();

	return actualNum;
}

Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	// 现在cc中找一下有没有管理空间非空的span
	Span* it = list.Begin();
	while (it != list.End())
	{
		if (it->_freeList != nullptr) // 找到管理空间非空的span
			return it;
		else // 没找到继续往下找
			it = it->_next;
	}

	// 解掉桶锁，让其他向该cc桶进行操作的线程能拿到锁
	list.m_mutex.unlock();

	// 走到这就是cc中没有找到管理空间非空的span

	// 将size转换成匹配的页数，以供pc提供一个合适的span
	size_t k = SizeClass::NumMovePage(size);

	// 解决死锁的方法三：在调用NewSpan的地方加锁
	PageCache::GetInstance()->_pageMtx.lock();
	// 调用NewSpan获取一个全新span
	Span* span = PageCache::GetInstance()->NewSpan(k);
	PageCache::GetInstance()->_pageMtx.unlock();

	/* 这里要强转一下，因为_pageID时PageID类型(size_t或者
	unsigned long long)的，不能直接赋值给指针*/
	char* start = (char*)(span->_pageID << PAGE_SHIFT);
	char* end = (char*)(start + (span->_n << PAGE_SHIFT));

	// 开始切分span管理的空间

	span->_freeList = start; //管理的空间放到span->_freeList中

	void* tail = start; // 起初让tail指向start
	start += size;	// start往后移一块，方便控制循环

	int i = 0;
	// 链接各个块
	while (start < end)
	{
		++i;
		ObjNext(tail) = start;
		start += size;
		tail = ObjNext(tail);
	}
	ObjNext(tail) = nullptr; // 记得要把最后一位置空

	// 切好span以后，需要把span挂到cc对应下标的桶里面去
	list.m_mutex.lock(); // span挂上去之前加锁
	list.PushFront(span);

	return span;
}