#pragma once

#include"Common.h"

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

	// 通过页地址找到span
	Span* MapObjectToSpan(void* obj);

	// 管理cc还回来的span
	void ReleaseSpanToPageCache(Span* span);

private:
	SpanList _spanLists[PAGE_NUM]; // pc中的哈希

	// 哈希映射，用来快速通过页号找到对应span
	//std::unordered_map<PageID, Span*> _idSpanMap;
	TCMalloc_PageMap1<32 - PAGE_SHIFT> _idSpanMap;

	ObjectPool<Span> _spanPool; // 创建span的对象池
public:
	// 这里锁可以专门给一个接口，但是我就直接公有了
	std::mutex _pageMtx; // pc整体的锁

private: // 单例，构造私有，拷构、拷赋去掉
	PageCache()
	{}

	PageCache(const PageCache& pc) = delete;
	PageCache& operator = (const PageCache& pc) = delete;

	static PageCache _sInst; // 单例类对象
};