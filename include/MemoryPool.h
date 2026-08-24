#pragma once
#include "ThreadCache.h"

struct MemoryPool
{
    static void* allocate(size_t size)
    {
        return ThreadCache::getInstance().allocate(size);
    }

    static void deallocate(void* ptr, size_t size)
    {
        return ThreadCache::getInstance().deallocate(ptr, size);
    }
};