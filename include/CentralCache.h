#pragma once
#include"Common.h"

class CentralCache
{
public:
	// 单例接口
	static CentralCache* GetInstance()
	{
		return &_sInst;
	}

	// cc从自己的_spanLists中为tc提供tc所需要的块空间
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);
		/*start和end表示cc提供的空间的开始结尾，输出型参数*/
		/*batchNum表示tc需要多少块size大小的空间*/
		/*size表示tc需要的单块空间的大小*/
		/*返回值是cc实际提供的小块空间个数*/

	// 获取一个管理空间不为空的span
	Span* GetOneSpan(SpanList& list, size_t size);

	// 将tc还回来的多块空间放到span中
	void ReleaseListToSpans(void* start, size_t size);

private:
	// 单例，去掉构造、拷构和拷赋
	CentralCache()
	{}

	CentralCache(const CentralCache& copy) = delete;
	CentralCache& operator =(const CentralCache& copy) = delete;

private:
	SpanList _spanLists[FREE_LIST_NUM]; // 哈希桶中挂的是一个一个的Span
	static CentralCache _sInst; // 饿汉模式创建一个CentralCache
};