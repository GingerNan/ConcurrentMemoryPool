#pragma once
#include "Common.h"

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


template<class T>
class ObjectPool
{
public:
	// 申请一块T类型大小的空间
	T* New()
	{
		T* obj = nullptr;	//最终返回的空间

		if (m_freelist)
		{
			// m_freelist不为空，表示有回收的T大小的小块可以重复利用
			void* next = *(void**)m_freelist;
			obj = (T*)m_freelist;
			m_freelist = next;
			// 头删操作
		}
		else
		{
			// _memory中剩余空间小于T的大小的收再开空间
			if (m_remanentBytes < sizeof(T))	// 这样也会包含剩余空间为0的情况
			{
				m_remanentBytes = 128 * 1024;	//开辟128K的空间
				
				//m_memory = (char*)malloc(m_remanentBytes);
				// 右移13位，就是除以8KB，也就是得到的是16，这里就表示申请16页
				m_memory = (char*)SystemAlloc(m_remanentBytes >> 13);
				
				if (m_memory == nullptr)	//声请内存失败，抛异常
				{
					throw std::bad_alloc();
				}
			}

			obj = (T*)m_memory;		//给定一个T类型的大小
			// 判断一下T的大小，小于指针就给一个指针大小，大于指针就还是T的大小
			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			m_memory += objSize;	// m_memory后移一个T类型的大小
			m_remanentBytes -= objSize;	// 空间给出后m_remanetBytes减少了T类型的大小
		}

		new(obj)T;	// 通过定位new调用构造函数进行初始化
		return obj;
	}

	void Delete(T* obj)		// 回收回来的小空间
	{
		// 显示调用析构函数进行清理工作
		obj->~T();

		// 头插
		*(void**)obj = m_freelist;	// 新块指向就旧块(或空)
		m_freelist = obj;			// 头指针指向新块
	}

private:
	char* m_memory = nullptr;		// 指向内存块的指针
	void* m_freelist = nullptr;		// 自由链表，用来链接归还的空闲空间
	size_t m_remanentBytes = 0;		// 大块内存在切分过程中剩余字节数
};