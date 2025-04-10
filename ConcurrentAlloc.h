#ifndef _CONCURRENT_ALLOC_H_
#define _CONCURRENT_ALLOC_H_
#include "ThreadCache.h"
#include "ObjectPool.h"

// 其实就是tcmalloc，线程调用这个函数申请空间
void* ConcurrentAlloc(size_t size)
{
	if (size > MAX_BYTES)
	{
		size_t alignSize = SizeClass::RoundUp(size); //先按照页大小对齐
		size_t k = alignSize >> PAGE_SHIFT; // 算出来对齐之后需要多少页

		PageCache::GetInstance()->_pageMtx.lock(); // 对pc中的span进行操作，加锁
		Span* span = PageCache::GetInstance()->NewSpan(k); // 直接向pc要
		PageCache::GetInstance()->_pageMtx.unlock(); // 解锁

		void* ptr = (void*)(span->_pageID >> PAGE_SHIFT); // 通过获得到的span来提供空间
		return ptr;
	}
	else // 申请空间小于256KB的就走原先的逻辑
	{
		/* 因为pTLSThreadCache是TLS的，每个线程都会有一个，且相互独立，所以不存在
	竞争pTLSThreadCache的问题，所以这里只需要判断一次就可以直接new，不纯在线程安全问题。*/
		if (pTLSThreadCache == nullptr)
		{
			//pTLSThreadCache = new ThreadCache;
			// 此时就相当于每个线程都有了一个ThreadCache对象

			// 用定长内存池来申请空间
			static ObjectPool<ThreadCache> objPool; // 静态的，一直存在
			objPool._poolMtx.lock(); // 加锁，不然多线程可能会申请到空指针
			pTLSThreadCache = objPool.New();
			objPool._poolMtx.unlock(); // 解锁
		}

		std::cout << "[" << std::this_thread::get_id() << "]_" << pTLSThreadCache << std::endl;

		return pTLSThreadCache->Allocate(size);
	}
}

// 线程调用这个函数来回收空间
void* ConcurrentFree(void* ptr)
{	/*这里第二个参数size后面会去掉的，
	这里只是为了让代码能跑才给的*/
	assert(ptr);

	/* 通过ptr找到对应的span，因为前面申请空间的
	时候已经保证了维护的空间首页地址已经映射了*/
	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size = span->_objSize;

	if (size > MAX_BYTES)
	{
		PageCache::GetInstance()->_pageMtx.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->_pageMtx.unlock();
	}
	else
	{ // 不是大于256KB的就走tc
		pTLSThreadCache->Deallocate(ptr, size);
	}

	return NULL;
}

#endif // !_CONCURRENT_ALLOC_H_