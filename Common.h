#ifndef _COMMON_H_
#define _COMMON_H_

#include <iostream>
#include <vector>
#include <cassert>
#include <mutex>
#include <unordered_map>

#ifdef _WIN32
#include <Windows.h>	// Windows下的头文件
#else
// 这里是Linux相关的头文件，就不写出来了
#endif // _WIN32

// 直接上堆上按页申请空间
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// linux下brk mmap等
#endif

	if (ptr == nullptr)
		throw std::bad_alloc();

	return ptr;
}

static const size_t FREE_LIST_NUM = 208;	// 哈希表中自由链表个数
static const size_t	MAX_BYTES = 256 * 1024;	// ThreadCache单次申请的最大字节数
static const size_t	PAGE_NUM = 129;			// span的最大管理页数
static const size_t	PAGE_SHIFT = 13;		// 一页多少位，这里给一页8KB，就是13位

static void*& ObjNext(void* obj)
{
	return *(void**)obj;
}

// ThreadCache中的自由链表
class FreeList
{
public:
	// 向自由链表中头插，且插入多块空间
	void PushRange(void* start, void* end, size_t size)
	{
		ObjNext(end) = m_freeList;
		m_freeList = start;

		m_size += size;
	}

	// 获取当前桶中有多少块空间
	size_t Size()
	{
		return m_size;
	}

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

		++m_size; // 插入一块，m_size + 1
	}

	// 用来提供空间的
	void* Pop()		
	{
		// 头删
		assert(m_freeList);		//提供空间的前提是要有空间

		void* obj = m_freeList;
		m_freeList = ObjNext(obj);

		--m_size; // 去掉一块，m_size - 1

		return obj;
	}

	// FreeList当前未到上限时，能够申请的最大空间是多少
	size_t& MaxSize()
	{
		return m_maxSize;
	}
private:
	void* m_freeList = nullptr;	//自由链表，初始为空
	size_t m_maxSize = 1;	// 当前自由链表申请未达到上限时，能够申请的大块空间是多少
							// 初始值给1，表示第一次能申请的就是1块
							// 到了上线之后_maxSize这个值就作废了
	size_t m_size = 0;	// 当前自由链表中有多少块空间
};


typedef size_t PageID;

// 管理以页为单位的大块内存
struct Span
{
	PageID _pageID = 0;	// 页号
	size_t _n = 0;		// 页的数量

	Span* _next = nullptr;	// 前一个节点
	Span* _prev = nullptr;	// 后一个节点

	void* _freeList = nullptr;	// 大块内存切小链接起来，这样回收的内存也方便链接
	size_t _usecount = 0;	// 使用计数，==0 说明所有对象都回来了

	bool _isUse = false; // 判断当前span是在cc中还是在pc中
};

class SpanList
{
public:
	SpanList()
	{	// 构造函数中搞哨兵位头节点
		m_head = new Span;

		// 因为是双向循环的，所以都指向m_head
		m_head->_next = m_head;
		m_head->_prev = m_head;
	}
	
	// 判空
	bool Empty()
	{
		// 带头双向循环空的时候_head指向自己
		return m_head == m_head->_next;
	}

	// 头插
	void PushFront(Span* span)
	{
		Insert(Begin(), span);
	}

	// 删除掉第一个span
	Span* PopFront()
	{
		// 先获取到_head后面的第一个span
		Span* front = m_head->_next;
		// 删除掉这个span，直接复用Erase
		Erase(front);

		// 返回原来的第一个Span
		return front;
	}

	// 头结点
	Span* Begin()
	{
		return m_head->_next;
	}

	// 尾节点
	Span* End()
	{
		return m_head;
	}

	// 在pos前面插入 prt
	void Insert(Span* pos, Span* ptr)
	{
		assert(pos);	// pos不为空
		assert(ptr);	// ptr不为空

		// 插入相关逻辑
		Span* prev = pos->_prev;

		prev->_next = ptr;
		ptr->_prev = prev;

		ptr->_next = pos;
		pos->_prev = ptr;
	}

	void Erase(Span* pos)
	{
		assert(pos);	// pos不为空
		assert(pos != m_head);	// pos不能是哨兵位

		Span* prev = pos->_prev;
		Span* next = pos->_next;

		prev->_next = next;
		next->_prev = prev;

		/*pos节点不需要调用delete删除，因为
		pos节点的Span需要回收，而不是直接删除掉*/

		// 回收相关逻辑
	}
public:
	std::mutex m_mutex;	//每个CentralCache中的哈希桶都要有一个桶锁

private:
	Span* m_head;	// 哨兵位头节点
};

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

	// 块页匹配算法
	static size_t NumMovePage(size_t size) // size表示一块的大小
	{
		// 当cc中没有span提供小块空间时，cc就需要向pc申请一块span，此时需要根据一块空间的大小类匹配
		// 出一个维护页空间比较合适的span，以保证span为size后尽量不浪费或不足够还再频繁申请相同大小的span

		// NumMoveSize时算出tc向cc申请size大小的块时的单次最大申请块数
		size_t num = NumMoveSize(size);

		// num * size 就是单次申请最大空间大小
		size_t npage = num * size;

		/*PAGE_SHIFT表示一页要占用多少位，比如一页8KB就是13位，这里右移
		其实就是除以页大小，算出来就是单次申请最大空间有多少页*/
		npage >>= PAGE_SHIFT;

		/*如果算出来为0，那就直接给1页，比如说size为8B时，num就是512，npage
		算出来就是4KB，那如果一页8KB，算出来直接为0了，意思就是半页的空间都够
		8B的单次申请的最大空间了，但是二进制中没有0.5，所以只能给1页*/
		if (npage == 0)
			npage = 1;

		return npage;
	}

	static size_t NumMoveSize(size_t size)
	{
		assert(size > 0);	// 不能申请0大小的空间

		// MAX_BYTES就是单个块的最大空间，也就是256KB
		size_t num = MAX_BYTES / size;	// 这里除之后先简单控制一下

		if (num > 512)
		{
			/*比如说单词申请的是8B，256KB除以8B得到的是一个三万多的数，
			那样单次上限三万多块太多了，直接开到三万多可能会造成很多浪费
			的空间，不太现实，所以该小一点*/
			num = 512;
		}

		// 如果说除了之后特别小，比2小，那么就调成2
		if (num < 2)
		{
			/*比如说单次申请的是256KB，那除得1，如果256KB上限一直是1，
			那么就有点太少了，可能线程要的是4个256KB，那将num改成2就可
			以少调用几次，也就会少几次开销，到那时也不能太多，256KB空间
			是很大的，num太高了不太现实，可能会出现浪费*/
			num = 2;
		}

		// [2，521]，一次批量移动多少个对象的(慢启动)上限值
		// 小对象一次批量上限高
		// 小对象一次批量上限低

		return num;
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
		return ((size + (static_cast<unsigned long long>(1) << align_shift) - 1) >> align_shift) - 1;
		// 这里_Index计算的是当前size所在区域的第几个下标，所以Index的返回值需要加上前面所有区域的哈希桶的个数
	}
};

#endif // !_COMMON_H_