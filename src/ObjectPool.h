#pragma once

/*定长内存池*/

#include"Common.h"

template<class T>
class ObjectPool
{
public:
	T* New() // 申请一个T类型大小的空间
	{
		T* obj = nullptr; // 最终返回的空间
		
		if (_freelist)
		{ // _freelist不为空，表示有回收的T大小的小块可以重复利用
			void* next = *(void**)_freelist;
			obj = (T*)_freelist;
			_freelist = next;
			// 头删操作
		}
		else
		{ // 自由链表中没有块，也就没有可以重复利用的空间
			// _memory中剩余空间小于T的大小的时候再开空间
			if (_remanentBytes < sizeof(T)) // 这样也会包含剩余空间为0的情况
			{
				_remanentBytes = 128 * 1024; // 开128K的空间
				//_memory = (char*)malloc(_remanentBytes);
				
				// 右移13位，就是除以8KB，也就是得到的是16，这里就表示申请16页
				_memory = (char*)SystemAlloc(_remanentBytes >> PAGE_SHIFT); 
				
				if (_memory == nullptr) // 开失败了抛异常
				{
					throw std::bad_alloc();
				}
			}

			obj = (T*)_memory; // 给定一个T类型的大小
			// 判断一下T的大小，小于指针就给一个指针大小，大于指针就还是T的大小
			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			_memory += objSize; // _memory后移一个T类型的大小
			_remanentBytes -= objSize; // 空间给出后_remanetBytes减少了T类型的大小
		}
		
		new(obj)T; // 通过定位new调用构造函数进行初始化

		if(obj == nullptr)
		{
			int x = 0;
		}
		return obj;
	}

	void Delete(T* obj) // 回收还回来的小空间
	{
		// 显示调用析构函数进行清理工作
		obj->~T();

		// 头插
		*(void**)obj = _freelist; // 新块指向旧块(或空)
		_freelist = obj; // 头指针指向新块
	}

private:
	char* _memory = nullptr; // 指向内存块的指针
	size_t _remanentBytes = 0; // 大块内存在切分过程中的剩余字节数
	void* _freelist = nullptr; // 自由链表，用来连接归还的空闲空间
public:
	std::mutex _poolMtx; // 防止ThreadCache申请时申请到空指针
};

//
//struct TreeNode // 一个树结构的节点，等会申请空间的时候就用这个树节点来申请
//{
//	int _val;
//	TreeNode* _left;
//	TreeNode* _right;
//
//	TreeNode()
//		:_val(0)
//		, _left(nullptr)
//		, _right(nullptr)
//	{}
//};
//
//void TestObjectPool() // malloc和当前定长内存池性能对比
//{
//	// 申请释放的轮次
//	const size_t Rounds = 5;
//
//	// 每轮申请释放多少次
//	const size_t N = 100000;
//
//	// 这里总共申请和释放的次数就是Rounds * N次，测试这么些次谁更快
//
//	std::vector<TreeNode*> v1;
//	v1.reserve(N);
//
//	// 测试malloc的性能
//	size_t begin1 = clock();
//	for (size_t j = 0; j < Rounds; ++j)
//	{
//		for (int i = 0; i < N; ++i)
//		{
//			v1.push_back(new TreeNode); // 这里虽然用的是new，但是new底层用的也是malloc
//		}
//		for (int i = 0; i < N; ++i)
//		{
//			delete v1[i]; // 同样的，delete底层也是free
//		}
//		v1.clear(); // 这里clear作用就是将vector中的内容清空，size置零，
//		// 但capacity保持不变，这样才能循环上去重新push_back
//	}
//	size_t end1 = clock();
//
//
//	std::vector<TreeNode*> v2;
//	v2.reserve(N);
//
//	// 定长内存池，其中申请和释放的T类型就是树节点
//	ObjectPool<TreeNode> TNPool;
//	size_t begin2 = clock();
//	for (size_t j = 0; j < Rounds; ++j)
//	{
//		for (int i = 0; i < N; ++i)
//		{
//			v2.push_back(TNPool.New()); // 定长内存池中的申请空间
//		}
//		for (int i = 0; i < N; ++i)
//		{
//			TNPool.Delete(v2[i]); // 定长内存池中的回收空间
//		}
//		v2.clear();// 这里clear作用就是将vector中的内容清空，size置零，
//		// 但capacity保持不变，这样才能循环上去重新push_back
//	}
//	size_t end2 = clock();
//
//
//	cout << "new cost time:" << end1 - begin1 << endl; // 这里可以认为时间单位就是ms
//	cout << "object pool cost time:" << end2 - begin2 << endl;
//}
//
////template<size_t N>
////class ObjectPool
////{
////
////};