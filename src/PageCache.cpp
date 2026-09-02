#include "PageCache.h"
#include <sys/mman.h>
#include <cstring>
#include <mutex>
//#include <windows.h>
#include <iostream>

void *PageCache::allocateSpan(size_t numPages)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 查找第一个页数大于等于numPages的空闲Span
    auto it = m_freeSpans.lower_bound(numPages);
    if (it != m_freeSpans.end())
    {
        //std::cout << "命中PageCache\n";
        Span* span = it->second;

        // 移除取出的span
        if (span->next)
        {
            it->second = span->next;
        }
        else
        {
            m_freeSpans.erase(it);
        }
        
        // 如果span大于需要的numPages则进行分割
        if (span->pageNums > numPages)
        {
            Span* newSpan = new Span;
            newSpan->pageAddr = static_cast<char*>(span->pageAddr) + numPages * PAGE_SIZE;
            newSpan->pageNums = span->pageNums - numPages;
            
            // 超出部分放回空闲Span*列表头部，并记录地址到Span*的映射
            auto& list = m_freeSpans[newSpan->pageNums];
            newSpan->next = list;
            list = newSpan;
            m_spanMap[newSpan->pageAddr] = span;

            span->pageNums = numPages;
        }

        m_spanMap[span->pageAddr] = span;
        return span->pageAddr;
    }

    // 没有合适的span，向系统申请
    void* ptr = systemAlloc(numPages);
    if (!ptr) return nullptr;

    // 创建并记录新的Span
    Span* span = new Span;
    span->pageAddr = ptr;
    span->pageNums = numPages;
    span->next = nullptr;
    m_spanMap[ptr] = span;

    return ptr;
}

void PageCache::deallocateSpan(void *ptr, size_t numPages)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    //std::cout << "ReturnPage: " << ptr << "\n";

    // 查找对应的span
    auto it = m_spanMap.find(ptr);
    if (it == m_spanMap.end()) return;

    Span* span = it->second;

    // 尝试合并相邻的span
    void* nextAddr = static_cast<char*>(ptr) + numPages * PAGE_SIZE;
    auto nextIt = m_spanMap.find(nextAddr);
    
    if (nextIt != m_spanMap.end())
    {
        // 1. 在空闲链表中查找nextSpan，如果找到移除之
        Span* nextSpan = nextIt->second;
        bool found = false;
        auto& nextList = m_freeSpans[nextIt->second->pageNums];

        // 检查是否是头节点
        if (nextList == nextSpan)
        {
            nextList = nextSpan->next;
            found = true;
        }
        else if (nextList) // 只有在链表非空时才遍历
        {
            Span* prev = nextList;
            while (prev->next)
            {
                if (prev->next == nextSpan)
                {   
                    // 将nextSpan从空闲链表中移除
                    prev->next = nextSpan->next;
                    found = true;
                    break;
                }
                prev = prev->next;
            }
        }
        
        // 2. 在找到nextSpan的情况下进行合并
        if (found)
        {
            span->pageNums += nextSpan->pageNums;
            delete nextSpan;
            m_spanMap.erase(nextAddr);
        }
    }

    // 放入空闲的Span链表
    auto& list = m_freeSpans[span->pageNums];
    span->next = list;
    list = span;
}

void *PageCache::systemAlloc(size_t numPages)
{
    size_t size = numPages * PAGE_SIZE;
    
    // 使用mmap 系统调用分配一块匿名的、私有的、可读可写的内存区域。
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    //void* ptr = VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    
    if (ptr == nullptr) return nullptr;

    //std::cout << "VritualAlloc: " << ptr << "\n";

    return ptr;
}

PageCache::~PageCache()
{
    for (auto spanPair: m_freeSpans)
    {
        Span* span = spanPair.second;
        while (span)
        {
            Span* next = span->next;
            munmap(span->pageAddr, span->pageNums);
            //VirtualFree(span->pageAddr, 0, MEM_RELEASE);
            delete span;
            span = next;
        }
    }
}
