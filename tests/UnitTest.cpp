#include"ConcurrentAlloc.h"

// 线程1执行方法
void Alloc1()
{// 两个线程调用ConncurrentAlloc测试能跑通不
	for (int i = 0; i < 5; ++i)
	{
		ConcurrentAlloc(6);
	}
}

// 线程2执行方法
void Alloc2()
{// 两个线程调用ConncurrentAlloc测试能跑通不
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

	cout << ptr1 << endl;
	cout << ptr2 << endl;
	cout << ptr3 << endl;
	cout << ptr4 << endl;
	cout << ptr5 << endl;
}

void ConcurrentAllocTest2()
{
	for (int i = 0; i < 1024; ++i)
	{
		void* ptr = ConcurrentAlloc(5);
		cout << ptr << endl;
	}

	void* ptr = ConcurrentAlloc(3);
	cout << "-----" << ptr << endl;
}

void TestConcurrentFree1()
{
	void* ptr1 = ConcurrentAlloc(5);
	void* ptr2 = ConcurrentAlloc(8);
	void* ptr3 = ConcurrentAlloc(4);
	void* ptr4 = ConcurrentAlloc(6);
	void* ptr5 = ConcurrentAlloc(3);
	void* ptr6 = ConcurrentAlloc(3);
	void* ptr7 = ConcurrentAlloc(3);

	ConcurrentFree(ptr1);
	ConcurrentFree(ptr2);
	ConcurrentFree(ptr3);
	ConcurrentFree(ptr4);
	ConcurrentFree(ptr5);
	ConcurrentFree(ptr6);
	ConcurrentFree(ptr7);
}

void MultiThreadAlloc1()
{
	std::vector<void*> v;
	for (size_t i = 0; i < 7; ++i)
	{
		void* ptr = ConcurrentAlloc(6);
		v.push_back(ptr);
	}

	for (auto e : v)
	{
		ConcurrentFree(e);
	}
}

void MultiThreadAlloc2()
{
	std::vector<void*> v;
	for (size_t i = 0; i < 7; ++i)
	{
		void* ptr = ConcurrentAlloc(16);
		v.push_back(ptr);
	}

	for (int i = 0; i < 7; ++i)
	{
		ConcurrentFree(v[i]);
	}
}
 
void TestMultiThread()
{
	std::thread t1(MultiThreadAlloc1);
	std::thread t2(MultiThreadAlloc2);

	t1.join();
	t2.join();
}

void TestAddressShift()
{
	// 两个页号
	PageID id1 = 2000;
	PageID id2 = 2001;
	// 通过页号找id1页的页内偏移
	char* p1 = (char*)(id1 << PAGE_SHIFT);
	char* p2 = (char*)(id2 << PAGE_SHIFT);
	while (p1 < p2)
	{
		cout << (void*)p1 << ":" << ((PageID)p1 >> PAGE_SHIFT) << endl;
		p1 += 8;
	}
}

void BigAlloc()
{
	void* p1 = ConcurrentAlloc(257 * 1024);
	ConcurrentFree(p1);

	void* p2 = ConcurrentAlloc(129 * 8 * 1024);
	ConcurrentFree(p2);
}

int main()
{
	//BigAlloc();

	//AllocTest();
	//ConcurrentAllocTest1();
	//ConcurrentAllocTest2();

	//TestAddressShift();

	//TestConcurrentFree1();
	//TestMultiThread();

	//void* ptr = ConcurrentAlloc(500 * 1024);
	return 0;
}

//#include<iostream>
//using std::min;
//#include<Windows.h>
//
//int main()
//{
//	int a = std::min(10, 20);
//
//	return 0;
//}

//#include <stdio.h>
//#include <cstdlib>
//
//int main() {
//    int a = 10;
//    int b = 20;
//    a = min(a, b);
//    printf("The minimum value is: %d\n", min);
//    return 0;
//}