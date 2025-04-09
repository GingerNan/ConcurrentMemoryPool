#ifndef _CONCURRENT_ALLOC_H_
#define _CONCURRENT_ALLOC_H_
#include "ThreadCache.h"

// 其实就是tcmalloc，线程调用这个函数申请空间
void* ConcurrentAlloc(size_t size)
{
	std::cout << "[" << std::this_thread::get_id() << "]_" << pTLSThreadCache << std::endl;

	/* 因为pTLSThreadCache是TLS的，每个线程都会有一个，
	* 且相互独立，所以不存在竞争pTLSThreadCache的问题，
	* 所以这里只需要判断一次就可以直接new，不纯在线程安全问题。
	*/
	if (pTLSThreadCache == nullptr)
	{
		pTLSThreadCache = new ThreadCache;
		// 此时就相当于每个线程都有了一个ThreadCache对象
	}
	return pTLSThreadCache->Allocate(size);
}

// 线程调用这个函数来回收空间
void* ConcurrentFree(void* obj, size_t size)
{
	assert(obj);
	pTLSThreadCache->Deallocate(obj, size);

	return NULL;
}

#endif // !_CONCURRENT_ALLOC_H_