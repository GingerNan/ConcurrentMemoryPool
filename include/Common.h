#pragma once

#include<cassert>

#include<iostream>
#include<thread>
#include<mutex>

#include<unordered_map>
#include<vector>


using std::vector;
using std::cout;
using std::endl;

static const size_t FREE_LIST_NUM = 208; // 哈希表中自由链表个数
static const size_t MAX_BYTES = 256 * 1024; // ThreadCache单次申请的最大字节数
static const size_t PAGE_NUM = 129; // span的最大管理页数
static const size_t PAGE_SHIFT = 13; // 一页多少位，这里给一页8KB，就是13位

// 注意下面size_t的大小会随着平台位数发生变化，32位下size_t是unsigned int（4字节），64位下 是unsigned __int64（8字节）
// 所以不需要进行预处理这里的_pageID的类型，所以下面的条件编译其实不用搞，只需要typedef size_t PageID就够了

typedef size_t PageID;

//#ifdef _WIN64
//	typedef unsigned long long PageID;
//#elif _WIN32
//	typedef size_t PageID;
//#else
//	// Linux下的
//#endif



#ifdef _WIN32
	#include<Windows.h> // Windows下的头文件
#else
	// 这里是Linux相关的头文件，我就不写出来了
#endif // _WIN32

// 直接去堆上按页申请空间
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32 // Windows下的系统调用接口
	void* ptr = VirtualAlloc(0, kpage << PAGE_SHIFT, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// linux下brk mmap等
#endif

	if (ptr == nullptr)
		throw std::bad_alloc();

	return ptr;
}

// 直接去堆上释放空间
inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	// sbrk unmmap等
#endif
}


/* ObjNext如果没有引用，返回的是一个右值，因为ObjNext返回值是一个拷贝，是一个临时
	对象，而临时对象具有常属性，不能被修改，也就是一个右值，右值无法进行赋值操作 */
static void*& ObjNext(void* obj) // obj的头4/8个字节
{ // 函数用static修饰，防止多个.cpp文件重复包含该Common头文件导致链接时产生冲突
	return *(void**)obj;
}

class SizeClass; // 这里要声明一下，不然PageMap中用到了SizeClass会报错，或者直接将PageMap放到最后面引用


// 这里头文件要放到这，不然上面的函数ObjectPool中没有，就会报错
#include"ObjectPool.h"
#include"PageMap.h"

class FreeList // ThreadCache中的自由链表
{
public:
	// 获取当前桶中有多少块空间
	size_t Size()
	{
		return _size;
	}

	// 删除掉桶中n个块（头删），并把删除的空间作为输出型参数返回
	void PopRange(void*& start, void*& end, size_t n)
	{
		// 删除块数不能超过size块
		assert(n <= _size);

		start = end = _freeList;

		for (size_t i = 0; i < n - 1; ++i)
		{
			end = ObjNext(end);
		}

		_freeList = ObjNext(end);
		ObjNext(end) = nullptr;
		_size -= n;
	}

	// 向自由链表中头插，且插入多块空间
	void PushRange(void* start, void* end, size_t size)
	{
		ObjNext(end) = _freeList;
		_freeList = start;

		_size += size;
	}

	bool Empty() // 判断哈希桶是否为空
	{
		return _freeList == nullptr;
	}

	void Push(void* obj) // 用来回收空间的
	{ // 头插
		assert(obj); // 插入非空空间

		ObjNext(obj) = _freeList;
		_freeList = obj;

		++_size; // 插入一块，size + 1
	}

	void* Pop() // 用来提供空间的
	{ // 头删
		assert(_freeList); // 提供空间的前提是要有空间

		void* obj = _freeList;
		_freeList = ObjNext(obj);

		--_size; // 去掉一块，_size - 1

		return obj;
	}

	// FreeList当前未到上限时，能够申请的最大块空间是多少
	size_t& MaxSize()
	{
		return _maxSize;
	}

private:
	void* _freeList = nullptr; // 自由链表，初始为空
	size_t _maxSize = 1; // 当前自由链表申请未达到上限时，能够申请的最大块空间是多少
						 // 初始值给1，表示第一次能申请的就是1块
						 // 到了上限之后_maxSize这个值就作废了
	size_t _size = 0; // 当前自由链表中有多少块空间
};

struct Span // 以页为基本单位的结构体
{
public:
	PageID _pageID = 0; // 页号
	size_t _n = 0; // 当前span管理的页的数量
	size_t _objSize = 0; // span管理页被切分成的块有多大

	void* _freeList = nullptr; // 每个span下面挂的小块空间的头结点
	size_t use_count = 0; // 当前span分配出去了多少个块空间


	Span* _prev = nullptr; // 前一个节点
	Span* _next = nullptr; // 后一个节点

	bool _isUse = false; // 判断当前span是在cc中还是在pc中
};

class SpanList
{
public:
	// 删除掉第一个span
	Span* PopFront()
	{
		// 先获取到_head后面的第一个span
		Span* front = _head->_next;
		// 删除掉这个span，直接复用Erase
		Erase(front);

		// 返回原来的第一个span
		return front;
	}

	// 判空
	bool Empty()
	{ // 带头双向循环空的时候_head指向自己
		return _head == _head->_next;
	}

	// 头插
	void PushFront(Span* span)
	{
		Insert(Begin(), span);
	}

	// 头结点
	Span* Begin()
	{
		return _head->_next;
	}

	// 尾结点
	Span* End()
	{
		return _head;
	}

	void Erase(Span* pos)
	{
		assert(pos); // pos不为空
		assert(pos != _head); // pos不能是哨兵位

		Span* prev = pos->_prev;
		Span* next = pos->_next;

		prev->_next = next;
		next->_prev = prev;

		/*pos节点不需要调用delete删除，因为
		pos节点的Span需要回收，而不是直接删掉*/
		// 回收相关逻辑
	}

	void Insert(Span* pos, Span* ptr)
	{ // 在pos前面插入ptr
		assert(pos); // pos不为空
		assert(ptr); // ptr不为空

		// 插入相关逻辑
		Span* prev = pos->_prev;

		prev->_next = ptr;
		ptr->_prev = prev;

		ptr->_next = pos;
		pos->_prev = ptr;
	}

	SpanList()
	{ // 构造函数中搞哨兵位头结点
		_head = new Span;

		// 因为是双向循环的，所以都指向_head
		_head->_next = _head;
		_head->_prev = _head;
	}

private:
	Span* _head; // 哨兵位头结点
public:
	std::mutex _mtx; // 每个CentralCache中的哈希桶都要有一个桶锁
};

class SizeClass
{
	// 线程申请size的对齐规则：整体控制在最多10%左右的内碎片浪费
	//	size范围				对齐数				对应哈希桶下标范围
	 // [1,128]					8B 对齐      		freelist[0,16)
	 // [128+1,1024]			16B 对齐  			freelist[16,72)
	 // [1024+1,8*1024]			128B 对齐  			freelist[72,128)
	 // [8*1024+1,64*1024]		1024B 对齐    		freelist[128,184)
	 // [64*1024+1,256*1024]	8*1024B 对齐  		freelist[184,208)
public:
	//// 计算每个分区对应的对齐后的字节数(普通写法)
	//static size_t _RoundUp(size_t size, size_t alignNum)
	//{									// alignNum是size对应分区的对齐数
	//	size_t res = 0;
	//	if (size % alignNum != 0)
	//	{ // 有余数，要多给一个对齐，比如size = 3，这里就是(3 / 8 + 1) * 8 = 8
	//		res = (size / alignNum + 1) * alignNum;
	//	}
	//	else
	//	{ // 没有余数，本身就能对齐，比如size = 8
	//		res = size;
	//	}

	//	return res;
	//}

	// 计算每个分区对应的对齐后的字节数(大佬写法)
	static size_t _RoundUp(size_t size, size_t alignNum)
	{									// alignNum是size对应分区的对齐数
		return ((size + alignNum - 1) & ~(alignNum - 1));
	}

	static size_t RoundUp(size_t size) // 计算对齐后的字节数，size为线程申请的空间大小
	{
		if (size <= 128)
		{ // [1,128] 8B
			return _RoundUp(size, 8);
		}
		else if (size <= 1024)
		{ // [128+1,1024] 16B
			return _RoundUp(size, 16);
		}
		else if (size <= 8 * 1024)
		{ // [1024+1,8*1024] 128B
			return _RoundUp(size, 128);
		}
		else if (size <= 64 * 1024)
		{ // [8*1024+1,64*1024] 1024B
			return _RoundUp(size, 1024);
		}
		else if (size <= 256 * 1024)
		{ // [64*1024+1,256*1024] 8 * 1024B
			return _RoundUp(size, 8 * 1024);
		}
		else
		{ // 单次申请空间大于256KB，直接按照页来对齐
			return _RoundUp(size, 1 << PAGE_SHIFT);
			// 这里直接给PAGE_SHIFT虽然说和前一个的8 * 1024KB一样，但
			// 是我们如果后续想要修改页大小的时候就直接修改PAGE_SHIFT
			// 这样就和前面的不一样了
		}
	}

	// 求size对应在哈希表中的下标（大佬写法）
	static inline size_t _Index(size_t size, size_t align_shift)
	{							/*这里align_shift是指对齐数的二进制位数。比如size为2的时候对齐数
								为8，8就是2^3，所以此时align_shift就是3*/
		return ((size + (1 << align_shift) - 1) >> align_shift) - 1;
		//这里_Index计算的是当前size所在区域的第几个下标，所以Index的返回值需要加上前面所有区域的哈希桶的个数
	}

	// 计算映射的哪一个自由链表桶（tc和cc用，二者映射规则一样）
	static inline size_t Index(size_t size)
	{
		assert(size <= MAX_BYTES);

		// 每个区间有多少个链
		static int group_array[4] = { 16, 56, 56, 56 };
		if (size <= 128)
		{ // [1,128] 8B -->8B就是2^3B，对应二进制位为3位
			return _Index(size, 3); // 3是指对齐数的二进制位位数，这里8B就是2^3B，所以就是3
		}
		else if (size <= 1024)
		{ // [128+1,1024] 16B -->4位
			return _Index(size - 128, 4) + group_array[0];
		}
		else if (size <= 8 * 1024)
		{ // [1024+1,8*1024] 128B -->7位
			return _Index(size - 1024, 7) + group_array[1] + group_array[0];
		}
		else if (size <= 64 * 1024)
		{ // [8*1024+1,64*1024] 1024B -->10位
			return _Index(size - 8 * 1024, 10) + group_array[2] + group_array[1]
				+ group_array[0];
		}
		else if (size <= 256 * 1024)
		{ // [64*1024+1,256*1024] 8 * 1024B  -->13位
			return _Index(size - 64 * 1024, PAGE_SHIFT) + group_array[3] +
				group_array[2] + group_array[1] + group_array[0];
		}
		else
		{
			assert(false);
		}
		return -1;
	}

	// tc向cc单次申请块空间的上限块数
	static size_t NumMoveSize(size_t size)
	{
		assert(size > 0); // 不能申请0大小的空间

		// MAX_BYTES就是单个块的最大空间，也就是256KB
		int num = MAX_BYTES / size; // 这里除之后先简单控制一下

		if (num > 512)
		{
			/*比如说单次申请的是8B，256KB除以8B得到的是一个三万多的
			数，那这样单次上限三万多块太多了，直接开到三万多可能会造
			成很多浪费的空间，不太现实，所以该小一点*/
			num = 512;
		}

		// 如果说除了之后特别小，比2小，那么就调成2
		if (num < 2)
		{
			/*比如说单次申请的是256KB，那除得1，如果256KB上限一直是1
			，那这样有点太少了，可能线程要的是4个256KB，那将num改成2
			就可以少调用几次，也就会少几次开销，但是也不能太多，256KB
			空间是很大的，num太高了不太现实，可能会出现浪费*/
			num = 2;
		}
		
		// [2, 512]，一次批量移动多少个对象的(慢启动)上限值
		// 小对象一次批量上限高
		// 小对象一次批量上限低

		return num;
	}

	// 块页匹配算法
	static size_t NumMovePage(size_t size)// size表示一块的大小
	{ // 当cc中没有span为tc提供小块空间时，cc就需要向pc申请一块span，此时需要根据一块空间的大小来匹配
	  // 出一个维护页空间较为合适的span，以保证span为size后尽量不浪费或不足够还再频繁申请相同大小的span

		// NumMoveSize是算出tc向cc申请size大小的块时的单次最大申请块数
		size_t num = NumMoveSize(size);
		
		// num * size就是单次申请最大空间大小
		size_t npage = num * size;

		/* PAGE_SHIFT表示一页要占用多少位，比如一页8KB就是13位，这里右
		 移其实就是除以页大小，算出来就是单次申请最大空间有多少页*/
		npage >>= PAGE_SHIFT;
	
		/*如果算出来为0，那就直接给1页，比如说size为8B时，num就是512，npage
		算出来就是4KB，那如果一页8KB，算出来直接为0了，意思就是半页的空间都
		够8B的单次申请的最大空间了，但是二进制中没有0.5，所以只能给1页*/
		if (npage == 0)
			npage = 1;
		
		return npage;
	}
};

