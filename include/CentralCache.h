#pragma once
#include "Common.h"
#include <cstddef>
#include <atomic>
#include <vector>
#include <array>
#include <chrono>

// 存储元数据
struct SpanTracker
{
    std::atomic<void*> spanAddr{nullptr};// 存储页的地址
    std::atomic<size_t> numPages{0};     // 页数
    std::atomic<size_t> useCount{0};     // 分配出的块数
    //std::atomic<size_t> freeCount{0};    // 追踪span中还有
};

class CentralCache
{
public:
    static CentralCache& getInstance()
    {
        static CentralCache centralCache;
        return centralCache;
    }

    void* fetchRange(size_t index); // 获取 index 对应的内存链表，个数固定
    void returnRange(size_t index, void* ptr, void* end); // 归还 index 对应的内存链表，传入[ptr, end]链表

    void assertReturn(); // 用于测试
private:
    CentralCache();
    // 返回需要获取的页数
    size_t getFetchPageNums(size_t bytes); 

    // 获取span信息
    SpanTracker* getSpanTracker(size_t index, void* blockAddr); 

    bool shouldPerformDelayedReturn(size_t index, size_t currentCount, std::chrono::steady_clock::time_point currentTime);
    void performDelayedReturn(size_t index);
    
private:
    // 用于同步的自旋锁
    std::array<std::atomic_flag, FREE_LIST_SIZE> m_locks;
    // 中心缓存的自由链表
    std::array<std::atomic<void*>, FREE_LIST_SIZE> m_centralFreeList; 

    // 延迟归还相关的成员变量
    static const size_t MAX_DELAY_COUNT = 48; // 最大延迟计数
    std::array<std::atomic<size_t>, FREE_LIST_SIZE> m_delayCounts; // 每个大小类的延迟计数
    std::array<std::chrono::steady_clock::time_point, FREE_LIST_SIZE> m_lastReturnTimes; // 上次归还时间
    static const std::chrono::milliseconds DELAY_INTERVAL; // 延迟间隔

    // 存储持有的页信息，每个span管理的页只分配固定大小的数据块
    std::array<std::array<SpanTracker, 1024>, FREE_LIST_SIZE> m_spanTrackers;
    std::array<std::atomic<size_t>, FREE_LIST_SIZE> m_spanCount{};
};