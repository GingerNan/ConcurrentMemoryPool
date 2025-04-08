#pragma once

#include <iostream>
#include <vector>
#include <cassert>

static const size_t FREE_LIST_NUM = 208;	// 哈希表中自由链表个数
static const size_t	MAX_BYTES = 256 * 1024;	// ThreadCache单次申请的最大字节数

static void*& ObjNext(void* obj)
{
	return *(void**)obj;
}

class FreeList
{
public:
	// 判断哈希桶是否为空
	bool Empty()
	{
		return m_freeList == nullptr;
	}

	// 用来回收空间
	void Push(void* obj)	
	{
		// 头插
		assert(obj);	//插入非空空间

		ObjNext(obj) = m_freeList;
		m_freeList = obj;
	}

	// 用来提供空间的
	void* Pop()		
	{
		// 头删
		assert(m_freeList);		//提供空间的前提是要有空间

		void* obj = m_freeList;
		m_freeList = ObjNext(obj);
		return obj;
	}
private:
	void* m_freeList = nullptr;	//自由链表，初始为空
};