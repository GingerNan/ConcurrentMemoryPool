#include "ThreadCache.h"

void* ThreadCache::Allocate(size_t size)
{
	assert(size <= MAX_BYTES);	// tc中单词只能申请不超过256KB的空间

	size_t alignSize = SizeClass::RoundUp(size);	// size对齐后的字节数
	size_t index = SizeClass::Index(size);			// size对应在哈希桶中的哪个桶

	if (!m_freeLists[index].Empty())
	{
		// 自由链表中不为空，可以直接从自由链表中获取空间
		return m_freeLists[index].Pop();
	}
	else
	{
		// 自由链表中为空，得到让 ThreadCache 向 CentralCache 申请空间
		return FetchFromCentralCache(index, alignSize);	// 至于为啥是这两个参数后面会讲到
	}
}

void ThreadCache::Deallocate(void* obj, size_t size)
{
	assert(obj); // 回收空间不能为空
	assert(size <= MAX_BYTES); // 回收空间大小不能超过256KB

	size_t index = SizeClass::Index(size);	// 找到size对应的自由链表
	m_freeLists[index].Push(obj);	// 用对应自由链表回收空间
}

void* ThreadCache::FetchFromCentralCache(size_t index, size_t alignSize)
{
#ifdef WIN32
	// 通过MaxSize和NumMoveSize来控制当前给tc提供多少块alignSize大小的空间
	size_t batchNum = min(m_freeLists[index].MaxSize(), SizeClass::NumMoveSize(alignSize));
	/*MaxSize表示index位置的自由链表单词申请未到上限时，能够申请的最大空间是多少*/
	/*NumMoveSize表示tc单次向cc申请alignSize大小的空间块的最多块数是多少*/
	/*二者取小，得到的就是本次要给tc提供多少块alignSize大小的空间*/
	/*比如说alignSize为8B，MaxSize为1，NumMoveSize为512，那就要给一块8B的空间*/
	/*也就是没到上限就给MaxSize，到了上限就给上限的NumMoveSize*/
#else
	// 其他系统中的用std
	size_t batchNum = std::min(m_freeLists[index].MaxSize(), SizeClass::NumMoveSize(alignSize));
#endif // WIN32

	if (batchNum == m_freeLists[index].MaxSize())
	{
		// 如果没有达到上限，那下次在申请这块空间的时候可以多申请一块
		m_freeLists[index].MaxSize()++;	// 下次多给一块
		// 这里就是慢开始反馈调节的核心
	}

	/*上面就是慢开始反馈调节算法*/

	// 输出型参数，返回之后的结果就是tc想要的空间
	void* start = nullptr;
	void* end = nullptr;

	// 返回值为实际获取到的块数
	size_t actulNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, alignSize);

	assert(actulNum >= 1); // actualNum一定是大于等于1的，这是FetchRangeObj能保证的

	if (actulNum == 1)
	{
		// 如果actulNum等于1，就直接将start返回给线程
		assert(start == end);
		return start;
	}
	else
	{
		// 如果actulNum大于1，就还要给tc对应位置插入[ObjNext(start)，end]的空间
		m_freeLists[index].PushRange(ObjNext(start), end, actulNum-1);

		// 给线程返回start所指空间
		return start;
	}

	return nullptr;
}
