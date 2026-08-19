#include <atomic>
#include <iostream>
#include <mutex>

#define BASE_SLOT_SIZE 8
#define MAX_SLOT_SIZE 512
#define MEMORY_POOL_NUM 64

struct Slot
{
    std::atomic<Slot*> next;
};

class MemoryPool
{
public:
    MemoryPool(size_t blockSize = 4096);
    ~MemoryPool();

    void init(size_t slotSize);

    void* allocate();
    void deallocate(void* ptr);

private:
    void allocateNewBlock();
    
private:
    size_t m_blockSize;//每个内存块的大小
    size_t m_slotSize;//每个槽的大小
    size_t m_slotCount;//每个内存块的槽数量
    Slot* m_slots;//内存块的槽数组

    Slot* m_freeList;//空闲槽的头指针
    Slot* m_currentSlot;//当前槽的指针
    Slot* m_lastSlot;//最后一个槽的指针

    Slot* m_firstBlock;//内存块的链表

    std::mutex m_mutexForFreeList; // 保证m_freeList在多线程操作的原子性
    std::mutex m_mutexForBlock; // 保证多线程情况下避免不必要的重复开辟内存导致的内存浪费行为
};

class HashBucket
{
public:
    static void initMemoryPool();
    static MemoryPool& getMemoryPool(size_t index);

    static void* useMemory(size_t size)
    {
        if (size <= 0)
            return nullptr;
        if (size > MAX_SLOT_SIZE)
            return operator new(size);
        return getMemoryPool((size + 7) / BASE_SLOT_SIZE - 1).allocate();
    }

    static void freeMomory(void* vp, size_t size)
    {
        if (!vp)
            return;
        if (size > MAX_SLOT_SIZE)
            return operator delete(vp);
        getMemoryPool((size + 7) / BASE_SLOT_SIZE - 1).deallocate(vp);
    }

    template<typename T, typename... Args>
    friend T* newElement(Args&&... args);

    template<typename T>
    friend void deleteElement(T* p);
};

template<typename T, typename... Args>
T* newElement(Args&&... args)
{
    T* p = nullptr;
    if ((p = reinterpret_cast<T*>(HashBucket::useMemory(sizeof(T)))) != nullptr)
        new(p) T(std::forward<Args>(args)...);
    return p;
}

template<typename T>
void deleteElement(T* p)
{
    if (p)
    {
        p->~T();
        HashBucket::freeMomory(p, sizeof(T));
    }
}

