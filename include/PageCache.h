#pragma once
#include "Common.h"
#include <map>
#include <mutex>



class PageCache
{
public:
    static const size_t PAGE_SIZE = 4096; // 4K页大小

    static PageCache& getInstance()
    {
        static PageCache pageCache;
        return pageCache;
    }

    void* allocateSpan(size_t numPages); // 获取nums个页，返回页起式地址
    void deallocateSpan(void* ptr, size_t numPages); // 归还nums个页，页起始地址为addr

private:
    PageCache() = default;
    ~PageCache();    

    void* systemAlloc(size_t numPages); // 向系统申请内存

private:
    struct Span
    {
        void* pageAddr;
        size_t pageNums;
        Span* next;
    };
    std::map<size_t, Span*> m_freeSpans; // 按页数管理空闲span
    std::map<void*, Span*> m_spanMap; // 页号到span的映射

    std::mutex m_mutex;
};