#pragma once
#include "Common.h"

// 管理以页为单位的大块内存
struct Span
{
	PageID _pageId = 0;	// 页号
	size_t _n = 0;		// 页的数量

	Span* _next = nullptr;
	Span* _prev = nullptr;

	void* _list = nullptr;	// 大块内存切小链接起来，这样回收的内存也方便链接
	size_t _usecount = 0;	// 使用计数，==0 说明所有对象都回来了
};

class CentralCache
{
private:
};
