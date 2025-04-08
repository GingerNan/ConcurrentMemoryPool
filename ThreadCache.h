#pragma once

#include "Common.h"

class ThreadCache
{
public:
	// 线程申请size大小的空间
	void* Allocate(size_t size);	

	// 回收线程中大小为size的obj空间
	void Deallocate(void* obj, size_t size);	

	// ThreadCache中空间不够时，向CentralCache申请空间的接口
	void* FetchFromCentralCache(size_t index, size_t alignSize);

private:
	FreeList m_freeLists[FREE_LIST_NUM];	//哈希，每个桶表示一个自由链表
};

// TLS的全局对象的指针，这样每个线程都能有一个独立的全局对象
static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;
// 注意要给程static的，不然当多个.cpp文件包含该文件的时候会发生链接错误


class SizeClass
{
public:
	static size_t RoundUp(size_t size)	// 计算对齐字节数
	{
		if (size <= 128)
		{	// [1,128] 8B
			return _RoundUp(size, 8);
		}
		else if (size <= 1024)
		{	// [128+1,1024] 16B
			return _RoundUp(size, 16);
		}
		else if (size <= 8 * 1024)
		{	// [1024+1,8*1024] 128B
			return _RoundUp(size, 128);
		}
		else if (size <= 64 * 1024)
		{	// [8*1024+1,64*1024] 1024B
			return _RoundUp(size, 1024);
		}
		else if (size <= 256 * 1024)
		{	// [64*1024+1,256*1024] 8*1024B
			return _RoundUp(size, 8 * 1024);
		}
		else
		{	// 不可能的情况，这里通过tc申请空间不会超过256KB
			assert(false);
			return -1;
		}
	}

	// 计算映射的哪一个自由链表桶
	static inline size_t Index(size_t size)
	{
		assert(size <= MAX_BYTES);

		// 每个区间有多少个链
		static int group_array[4] = { 16, 56, 56, 56 };
		if (size <= 128)
		{ // [1,128] 8B --> 8B就是2^3B，对应二进制位为3位
			return _Index(size, 3);	// 3是指对齐数的二进制位位数，这里8B就是2^3B，所以就是3
		}
		else if (size <= 1024)
		{ // [128+1, 1024] 16B --> 4位
			return _Index(size - 128, 4) + group_array[0];
		}
		else if (size <= 8 * 1024)
		{ // [8*1024, 64*1024] 1024B --> 7位
			return _Index(size - 1024, 7) + group_array[1] + group_array[0];
		}
		else if (size <= 64 * 1024)
		{ // [8*1024+1, 64*1024] 1024B --> 10位
			return _Index(size - 8 * 1024, 10) + group_array[2] + group_array[1];
		}
		else if (size <= 256 * 1024)
		{ // [64*1024+1, 256*1024] 8*1024B --> 13位
			return _Index(size - 64 * 1024, 13) + group_array[3] +
				group_array[2] + group_array[1] + group_array[0];
		}
		else
		{
			assert(false);
		}
		return -1;
	}

private:
	// 计算每个分区对应的对齐后的字节数
	// alignNum是size对应分区的对其数
	static size_t _RoundUp(size_t size, size_t alignNum)
	{
		size_t res = 0;
		if (size % alignNum != 0)
		{	// 有余数，要多给一个对齐，比如size=3，这里就是(3 / 8 + 1) * 8 = 8
			res = (size / alignNum + 1) * alignNum;
		}
		else
		{	// 没有余数，本身就能对齐，比如size = 8
			res = size;
		}
		return res;
	}

	// 求size对应在哈希表中的下标
	static inline size_t _Index(size_t size, size_t align_shift)
	{						/* 这里align_shift是指对齐数的二进制数。比如size为2的时候对齐数
							为8，8就是2 ^ 3，所以此时align_shift就是3*/
		return ((size + (1 << align_shift) - 1) >> align_shift) - 1;
		// 这里_Index计算的是当前size所在区域的第几个下标，所以Index的返回值需要加上前面所有区域的哈希桶的个数
	}
};