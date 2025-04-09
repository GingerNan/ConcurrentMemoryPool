#include <vector>
#include <thread>
#include "ObjectPool.h"
#include "ConcurrentAlloc.h"

struct TreeNode	// 一个树结构的节点，等会申请空间的时候就用这个树节点来申请
{
	int _val;
	TreeNode* _left;
	TreeNode* _right;

	TreeNode()
		:_val(0)
		,_left(nullptr)
		,_right(nullptr)
	{}
};

void TestObjectPool()	// malloc和当前定长内存池性能对比
{
	// 申请释放的轮次
	const size_t Rounds = 10;

	// 每轮申请释放多少次
	const size_t N = 1000000;

	// 这里总共申请和释放的次数就是Rounds * N次，测试这些次谁更快
	std::vector<TreeNode*> v1;
	v1.reserve(N);

	// 测试malloc的性能
	size_t begin1 = clock();
	for (size_t j = 0; j < Rounds; ++j)
	{
		for (int i = 0; i < N; ++i)
		{
			v1.push_back(new TreeNode);	// 这里虽然用的是new，但是new底层用的也是malloc
		}

		for (int i = 0; i < N; ++i)
		{
			delete v1[i];	// 同样的，delete底层也是free
		}
		v1.clear();	// 这里clear作用就是将vector中内容清空，size置零
		// 但capacity保持不变，这样才能循环上去重新push_back
	}
	size_t end1 = clock();

	std::vector<TreeNode*> v2;
	v2.reserve(N);

	// 定长内存池，其中申请和释放的T类型就是树节点
	ObjectPool<TreeNode> TNPool;
	size_t begin2 = clock();
	for (size_t j = 0; j < Rounds; ++j)
	{
		for (int i = 0; i < N; ++i)
		{
			v2.push_back(TNPool.New()); // 定长内存池中的申请空间
		}
		for (int i = 0; i < N; ++i)
		{
			TNPool.Delete(v2[i]); // 定长内存池中的回收空间
		}
		v2.clear();// 这里clear作用就是将vector中的内容清空，size置零，
		// 但capacity保持不变，这样才能循环上去重新push_back
	}
	size_t end2 = clock();

	std::cout << "new cost time:" << end1 - begin1 << std::endl; // 这里可以认为时间单位就是ms
	std::cout << "object pool cost time:" << end2 - begin2 << std::endl;
}

// 线程1执行方法
void Alloc1()
{ // 两个线程调用ConncurrentAlloc测试能跑通不
	for (int i = 0; i < 5; ++i)
	{
		ConcurrentAlloc(6);
	}
}

// 线程2执行方法
void Alloc2()
{ // 两个线程调用ConncurrentAlloc测试能跑通不
	for (int i = 0; i < 5; ++i)
	{
		ConcurrentAlloc(7);
	}
}

void AllocTest()
{
	std::thread t1(Alloc1);
	t1.join();

	std::thread t2(Alloc2);
	t2.join();
}

void ConcurrentAllocTest1()
{
	void* ptr1 = ConcurrentAlloc(5);
	void* ptr2 = ConcurrentAlloc(8);
	void* ptr3 = ConcurrentAlloc(4);
	void* ptr4 = ConcurrentAlloc(6);
	void* ptr5 = ConcurrentAlloc(3);

	std::cout << ptr1 << std::endl;
	std::cout << ptr2 << std::endl;
	std::cout << ptr3 << std::endl;
	std::cout << ptr4 << std::endl;
	std::cout << ptr5 << std::endl;
}

void ConcurrentAllocTest2()
{
	for (int i = 0; i < 1024; ++i)
	{
		void* ptr = ConcurrentAlloc(5);
		std::cout << ptr << std::endl;
	}

	void* ptr = ConcurrentAlloc(3);
	std::cout << "-------------" << ptr << std::endl;
}

void TestAddressShift()
{
	// 两个页号
	PageID id1 = 2000;
	PageID id2 = 2001;

	// 通过页号找到id1页的页内偏移
	char* p1 = (char*)(id1 << PAGE_SHIFT);
	char* p2 = (char*)(id2 << PAGE_SHIFT);

	while (p1 < p2)
	{
		std::cout << (void*)p1 << ":" << ((PageID)p1 >> PAGE_SHIFT) << std::endl;
		p1 += 8;
	}
}

int main()
{
	// TestObjectPool();

	ConcurrentAllocTest1();
}