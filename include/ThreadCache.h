#include "Common.h"

class ThreadCache
{
public:
    static ThreadCache& getInstance()
    {
        static ThreadCache threadCache;
        return threadCache;
    }

    void* allocate(size_t size);
    void deallocate(void* ptr, size_t size);
private:
    ThreadCache()
    {
        m_freeList.fill(nullptr);
        m_freeListSize.fill(0);
    }
    
    void* fetchFromCentralCache(size_t size);
    
    void returnToCentralCache(void* start, size_t);

    size_t getBatchNum(size_t size);
    
    bool shouldReturnToCentralCache(size_t index);

private:
    std::array<void*, FREE_LIST_SIZE> m_freeList;
    std::array<size_t, FREE_LIST_SIZE> m_freeListSize;
};