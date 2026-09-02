#include "ThreadCache.h"
#include "CentralCache.h"
#include <cstdlib>
#include <assert.h>
#include <iostream>

// 大小类管理
class SizeClass 
{
public:
    static size_t getIndex(size_t bytes)
    {   
        // 确保bytes至少为ALIGNMENT
        bytes = std::max(bytes, ALIGNMENT);
        // 向上取整后-1
        return (bytes + ALIGNMENT - 1) / ALIGNMENT - 1;
    }
};

void *ThreadCache::allocate(size_t size)
{
    if (size == 0)
    {
        size = ALIGNMENT;
    }

    if (size > MAX_BYTES)
    {
        return malloc(size);
    }

    size_t index = SizeClass::getIndex(size);

    if (m_freeListSize[index] != 0) 
    {
        void* ptr = m_freeList[index];
        assert(ptr != nullptr);
        m_freeList[index] = SLL_Next(ptr);
        m_freeListSize[index]--;
        return ptr;
    }

    return fetchFromCentralCache(index);
}

void ThreadCache::deallocate(void *ptr, size_t size)
{
    if (ptr == nullptr) return;
    if (size > MAX_BYTES)
    {
        free(ptr);
        return;
    }

    SLL_SetNext(ptr, nullptr);

    size_t index = SizeClass::getIndex(size);

    SLL_SetNext(ptr, m_freeList[index]);
    m_freeList[index] = ptr;

    m_freeListSize[index]++;

    if (shouldReturnToCentralCache(index))
    {
        returnToCentralCache(size);
    }
}

void *ThreadCache::fetchFromCentralCache(size_t index)
{
    void* start = CentralCache::getInstance().fetchRange(index);
    if (!start) return nullptr;

    void* result = start;
    m_freeList[index] = SLL_Next(start);

    size_t batchNum = 0; // 计算实际获取到的内存块
    void* current = m_freeList[index];

    while (current != nullptr)
    {
        batchNum++;
        current = SLL_Next(current);
    }
    //assert(batchNum <= 16);
    m_freeListSize[index] = batchNum;

    return result;
}

bool ThreadCache::shouldReturnToCentralCache(size_t index)
{
    //return true;
    // 超过阈值后，回收自由链表
    static constexpr size_t threshold = 256;
    return (m_freeListSize[index] > threshold);
}

void ThreadCache::returnToCentralCache(size_t size, bool reserve)
{
    size_t index = SizeClass::getIndex(size);

    size_t batchNum = m_freeListSize[index];
    if (batchNum <= 0)
    {
        //assert(m_freeList[index] == nullptr);
        return;
    }

    size_t keepNum = 0; 
    if (reserve)
    {
        keepNum = batchNum / 4; // 保留一部分
        //if (keepNum < 8) keepNum = 0;
    }
    //size_t returnNum = batchNum - keepNum;

    // 计算分割节点
    void* start = m_freeList[index];
    void* end = m_freeList[index];
    void* splitNode = nullptr;


    if (end == nullptr)
    {
        return;
    }

    //return;
    size_t count = 0;
    while (SLL_Next(end) != nullptr)
    {
        ++count;
        if (keepNum == count)
            splitNode = end;
        end = SLL_Next(end);
    }

    if (keepNum == 0) // 无保留
    {
        m_freeList[index] = nullptr;
        m_freeListSize[index] = 0;
        CentralCache::getInstance().returnRange(index, start, end);
        return;
    }
    else if (splitNode == nullptr) // 全保留
    {
        // count < keepNum, 更新链表大小
        m_freeListSize[index] = count;
        return;
    }
    else // 保留部分
    {
        // 分割要返回和保留的部分
        void* nextNode = SLL_Next(splitNode);
        SLL_SetNext(splitNode, nullptr);
        m_freeListSize[index] = keepNum;
        CentralCache::getInstance().returnRange(index, nextNode, end);
    }
}