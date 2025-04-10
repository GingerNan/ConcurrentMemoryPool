#ifndef _CENTRAL_CACHE_H_
#define _CENTRAL_CACHE_H_

#include "Common.h"
#include "PageCache.h"

class CentralCache
{
public:
	// 单例接口
	static CentralCache* GetInstance()
	{
		return &m_sInst;
	}

	// cc从一个管理空间非空的span中拿出一段batchNum个size大小的块空间
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);
		/*start和end表示cc提供的空间的开始结尾，输出型参数*/
		/*n表示tc需要多少块size大小的空间*/
		/*size表示tc需要的单块空间的大小*/
		/*返回值是cc实际提供的空间大小*/

	// 获取一个管理空间不为空的span
	Span* GetOneSpan(SpanList& list, size_t size);

	// 将tc换回来的多块空间放到span中
	void ReleaseListToSpans(void* start, size_t size);

private:
	// 单例，去掉构造、拷贝和拷赋
	CentralCache() {}

	CentralCache(const CentralCache& copy) = delete;
	CentralCache& operator = (const CentralCache& copy) = delete;

private:
	SpanList m_spanLists[FREE_LIST_NUM];	// 哈希桶中挂的是一个一个的Span
	static CentralCache m_sInst;	// 饿汉模式创建一个CentralCache
};
#endif // !_CENTRAL_CACHE_H_
