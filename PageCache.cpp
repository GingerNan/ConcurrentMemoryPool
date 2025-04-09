#include "PageCache.h"

PageCache PageCache::_sInst;

Span* PageCache::NewSpan(size_t k)
{
	// 申请页数一定是在[1，PAGE_NUM - 1]这个范围内的
	assert(k > 0 && k < PAGE_NUM);

	//GetInstance()->_pageMtx.lock();
	// 1.K号桶中有span
	if (!_spanLists[k].Empty())
	{ // 直接返回该桶中的第一个span
		Span* span = _spanLists[k].PopFront();
		return span;
	}

	// 2.K号桶没有span，但后面的桶中有span
	for (size_t i = k + 1; i < PAGE_NUM; ++i)
	{
		// [k+1, PAGE_NUM -1]号桶中有没有span
		if (!_spanLists[i].Empty())
		{ // i号号桶中span，对该span进行切分
			
			// 获取到该桶中的span，起名就叫nSpan
			Span* nSpan = _spanLists[i].PopFront();

			// 将这个span切分成一个k页的和一个n-k页的span

			// Span的空间是需要新建的，而不是用当前内存池中的空间
			Span* kSpan = new Span;

			// 分一个k页的span
			kSpan->_pageID = nSpan->_pageID;
			kSpan->_n = k;

			// 和一个 n - k 页的span
			nSpan->_pageID += k;
			nSpan->_n -= k;

			// n - k页的放回对应哈希桶中
			_spanLists[nSpan->_n].PushFront(nSpan);

			return kSpan;
		}
	}

	// 3.K号桶和后面的桶中都没有span

	// 直接向系统申请128页的span
	void* ptr = SystemAlloc(PAGE_NUM - 1); // PAGE_NUM为129

	// 开一个新的span用来维护这块空间
	Span* bigSpan = new Span;

	/* 只需要修改_pageID和_n即可，
	系统调用接口申请空间的时候一定能保证申请的空间是对齐的 */
	bigSpan->_pageID = ((PageID)ptr) >> PAGE_SHIFT;
	bigSpan->_n = PAGE_NUM - 1;

	// 将这个span放到对应哈希桶中
	_spanLists[PAGE_NUM - 1].PushFront(bigSpan);

	// 递归再次申请k页的span，这次递归一定会走2的逻辑
	return NewSpan(k); // 复用代码
}

Span* PageCache::GetOneSpan(SpanList& list, size_t size)
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

	// 走到这里cc中没有找到管理空间非空的span

	return nullptr;
}