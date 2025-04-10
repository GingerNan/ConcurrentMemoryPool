#ifndef _PAGE_CACHE_H_
#define _PAGE_CACHE_H_

#include "Common.h"
#include "ObjectPool.h"

class PageCache
{
public:
	// 饿汉单例
	static PageCache* GetInstance()
	{
		return &_sInst;
	}

	// pc从_spanLists中拿出来一个k页的span
	Span* NewSpan(size_t k);

	// 获取一个管理空间非空的Span
	Span* GetOneSpan(SpanList& list, size_t size);

	// 通过页地址找到span
	Span* MapObjectToSpan(void* obj);

	// 管理cc还回来的span
	void ReleaseSpanToPageCache(Span* span);

private:
	PageCache() {}

	PageCache(const PageCache& pc) = delete;
	PageCache& operator = (const PageCache& pc) = delete;

	static PageCache _sInst;	//单例类对象

private:
	SpanList _spanLists[PAGE_NUM];	// pc中的哈希

	// 哈希映射，用来快速通过页号找到对应的span
	std::unordered_map<PageID, Span*> _idSpanMap;

	ObjectPool<Span> _spanPool; //创建span的对象池
public:
	std::mutex _pageMtx;	// pc整体的锁
};

#endif // 