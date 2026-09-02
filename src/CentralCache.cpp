#include "CentralCache.h"
#include "PageCache.h"
#include <thread>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <assert.h>
#include <iostream>

// 每次从PageCache获取span大小（以页为单位）
static const size_t SPAN_PAGES = 8;

const std::chrono::milliseconds CentralCache::DELAY_INTERVAL{1000};



CentralCache::CentralCache()
{
    for (auto& lock : m_locks)
    {
        lock.clear(std::memory_order_relaxed);
    }
    for (auto& ptr : m_centralFreeList)
    {
        ptr.store(nullptr, std::memory_order_relaxed);
    }
    for (auto& count : m_delayCounts)
    {
        count.store(0, std::memory_order_relaxed);
    }
    for (auto& time : m_lastReturnTimes)
    {
        time = std::chrono::steady_clock::now();
    }
    for (auto& spanCount : m_spanCount)
    {
        spanCount.store(0, std::memory_order_relaxed);
    }
    std::atomic_thread_fence(std::memory_order_release);
}

void* CentralCache::fetchRange(size_t index)
{
    // 超过最大索引时，向系统直接申请内存
    if (index >= FREE_LIST_SIZE)
        return nullptr;

    while (m_locks[index].test_and_set(std::memory_order_acquire))
    {
        std::this_thread::yield();
    }

    void* result = nullptr;
    try
    {
        result = m_centralFreeList[index].load(std::memory_order_relaxed);

        if (!result) // 如果中心缓存为空的情况，从页获取新的内存块
        {
            size_t size = (index + 1) * ALIGNMENT;
            size_t pageNums = getFetchPageNums(size);
            result = PageCache::getInstance().allocateSpan(pageNums);

            if (!result)
            {
                m_locks[index].clear(std::memory_order_release);
                return nullptr;
            }

            //std::cout << "fetchPage for PageCache. BaseAddr: " << result << "    Index: " << index << std::endl;

            // 将新获取的页切分成小块
            size_t blockNum = (pageNums * PageCache::PAGE_SIZE) / size;

            if (blockNum >= 1)
            {
                // 构建链表
                char* start = static_cast<char*>(result);

                constexpr size_t fixedStrategy = 16ULL; // 初始分配个数
                size_t useNums = std::min(blockNum, fixedStrategy);

                void *current, *next = result;
                for (size_t i = 1; i < useNums; ++i)
                {
                    current = start + (i - 1) * size;
                    next = start + i * size;
                    SLL_SetNext(current, next);
                }
                SLL_SetNext(next, nullptr);

                // 还有剩余容量，构建链表保留在CentralCache中
                if (useNums < blockNum)
                {
                    for (size_t i = useNums + 1; i < blockNum; ++i)
                    {
                        current = start + (i - 1) * size;
                        next = start + i * size;
                        SLL_SetNext(current, next);
                    }
                    SLL_SetNext(next, nullptr);

                    current = start + useNums * size;
                    m_centralFreeList[index].store(current, std::memory_order_release);
                }

                // 记录span信息，用于判断是否可以归还给PageCache。
                size_t trackerIndex = m_spanCount[index].fetch_add(1, std::memory_order_release);
                if (trackerIndex < m_spanTrackers[index].size())
                {
                    m_spanTrackers[index][trackerIndex].spanAddr.store(start, std::memory_order_relaxed);
                    m_spanTrackers[index][trackerIndex].numPages.store(pageNums, std::memory_order_relaxed);
                    m_spanTrackers[index][trackerIndex].useCount.store(useNums, std::memory_order_release);
                }
                else
                {
                    assert(false);
                }
            }
            else
            {
                assert(false);
            }
        }
        else
        {
            // 从链表中取出头节点
            constexpr size_t fixedStrategy = 16ULL; // 初始分配个数
            
            void* ptr = (result);
            for (size_t useNum = 1; useNum < fixedStrategy && ptr; ++useNum)
            {
                SpanTracker* tracker = getSpanTracker(index, ptr);
                if (tracker)
                {
                    tracker->useCount.fetch_add(1, std::memory_order_relaxed);
                }
                ptr = SLL_Next(ptr);
            }
            if (ptr)
            {
                // 链表末尾的结点也要统计所属Span信息
                SpanTracker* tracker = getSpanTracker(index, ptr);
                if (tracker)
                {
                    tracker->useCount.fetch_add(1, std::memory_order_relaxed);
                }
                m_centralFreeList[index].store(SLL_Next(ptr), std::memory_order_relaxed);
                SLL_SetNext(ptr, nullptr);
            }
            else // 个数不够，全部取走
            {
                m_centralFreeList[index].store(nullptr, std::memory_order_relaxed);
            }
        }
    }
    catch(...)
    {
        m_locks[index].clear(std::memory_order_release);
        throw;
    }
    
    m_locks[index].clear(std::memory_order_release);
    return result;
}

void CentralCache::returnRange(size_t index, void* start, void* end)
{
    if (index >= FREE_LIST_SIZE || !start)
        return;
    
    //size_t blockSize = (index + 1) * ALIGNMENT;
    
    while (m_locks[index].test_and_set(std::memory_order_acquire))
    {
        std::this_thread::yield();
    }

    try
    {
        // 1. 将归还的链表连接到中心缓存
        void* current = m_centralFreeList[index].load(std::memory_order_relaxed);
        SLL_SetNext(end, current);
        m_centralFreeList[index].store(start, std::memory_order_relaxed);

        while (start)
        {
            SpanTracker* tracker = getSpanTracker(index, start);
            if (tracker != nullptr)
            {
                tracker->useCount.fetch_sub(1, std::memory_order_relaxed);
            }
            else
            {
                assert(false);
            }
            if (start == end) break;
            start = SLL_Next(start);
        }

        // 2. 更新延迟计数
        size_t currentCount = m_delayCounts[index].fetch_add(1, std::memory_order_relaxed) + 1;
        auto currentTime = std::chrono::steady_clock::now();

        // 3. 检查是否需要执行延迟归还
        if (shouldPerformDelayedReturn(index, currentCount, currentTime))
        {
            performDelayedReturn(index);
        }
    }
    catch(...)
    {
        m_locks[index].clear(std::memory_order_release);
        throw;
    }
    
    m_locks[index].clear(std::memory_order_release);
}

void CentralCache::assertReturn()
{
    for (int i = 0; i < FREE_LIST_SIZE; ++i)
    {
        for (int j = 0; j < m_spanCount[i]; ++j)
        {
            //assert(m_spanTrackers[i][j].useCount == 0);
        }
    }
}


bool CentralCache::shouldPerformDelayedReturn(size_t index, size_t currentCount, std::chrono::steady_clock::time_point currentTime)
{   
    //return false;
    // 基于计数和时间的双重检查
    if (currentCount >= MAX_DELAY_COUNT)
    {
        return true;
    }

    auto lastTime = m_lastReturnTimes[index];
    return (currentTime - lastTime) >= DELAY_INTERVAL;
}

void CentralCache::performDelayedReturn(size_t index)
{
    // 重置延迟计数和更新最后归还时间
    m_delayCounts[index].store(0, std::memory_order_relaxed);
    m_lastReturnTimes[index] = std::chrono::steady_clock::now();


    void* head = m_centralFreeList[index];
    void* newHead = nullptr;
    void* prev = nullptr;
    void* current = head;

    
    // 从 freeList 中移除将要归还的块。
    while (current)
    {
        SpanTracker* span = getSpanTracker(index, current);

        if (span == nullptr)
        {
            // 非内存池分配
            assert(false);
            //std::cout << "取不到SpanTracker: " << current << std::endl;
        }
        else if (span->useCount.load(std::memory_order_relaxed) == 0) // 将要回收
        {            
            if (prev)
            {
                SLL_SetNext(prev, SLL_Next(current));
            }
        }
        else // 要保留下来
        {
            prev = current;
            if (newHead == nullptr)
                newHead = current;
        }
        current = SLL_Next(current);
    }
    m_centralFreeList[index].store(newHead, std::memory_order_release);
    
    // 归还给PageCache页，并将Span信息从SpanTrackers中移除
    int p = 0, spanCount = m_spanCount[index].load(std::memory_order_acquire);
    for (int i = 0; i < spanCount; ++i)
    { 
        size_t useCount = m_spanTrackers[index][i].useCount.load(std::memory_order_relaxed);
        if (useCount == 0)
        {
            void* spanAddr = m_spanTrackers[index][i].spanAddr.load(std::memory_order_relaxed);
            size_t numPages = m_spanTrackers[index][i].numPages.load(std::memory_order_relaxed);
            PageCache::getInstance().deallocateSpan(spanAddr, numPages);
        }
        if (useCount)
        {
            if (p != i)
            {
                void* spanAddr = m_spanTrackers[index][i].spanAddr.load(std::memory_order_relaxed);
                size_t numPages = m_spanTrackers[index][i].numPages.load(std::memory_order_relaxed);
                m_spanTrackers[index][p].spanAddr.store(spanAddr, std::memory_order_relaxed);
                m_spanTrackers[index][p].numPages.store(numPages, std::memory_order_relaxed);
                m_spanTrackers[index][p].useCount.store(useCount, std::memory_order_relaxed);
            }
            ++p;
        }
    }
    m_spanCount[index].store(p, std::memory_order_release);
}


size_t CentralCache::getFetchPageNums(size_t bytes)
{
    if (bytes <= SPAN_PAGES * PageCache::PAGE_SIZE)
    {
        // 小于等于32KB的请求，使用固定8页
        return SPAN_PAGES;
    }
    else
    {
        // 大于32KB的请求，按实际需求分配
        size_t numPages = (bytes + PageCache::PAGE_SIZE - 1) / PageCache::PAGE_SIZE;
        return numPages;
    }
}

SpanTracker* CentralCache::getSpanTracker(size_t index, void *blockAddr)
{
    for (size_t i = 0; i < m_spanCount[index].load(std::memory_order_relaxed); ++i)
    {
        void* spanAddr = m_spanTrackers[index][i].spanAddr.load(std::memory_order_relaxed);
        size_t numPages = m_spanTrackers[index][i].numPages.load(std::memory_order_relaxed);

        if (blockAddr >= spanAddr && blockAddr < static_cast<char*>(spanAddr) + numPages * PageCache::PAGE_SIZE)
            {
                return &m_spanTrackers[index][i];
            }
    }
    return nullptr;
}

