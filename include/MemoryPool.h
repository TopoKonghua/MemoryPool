#include <atomic>
#include <iostream>
#include <list>

#define BASE_SLOT_SIZE 8

struct Slot
{
    std::atomic<Slot*> next;
};

class MemoryPool
{
public:
    MemoryPool(size_t slotSize,size_t blockSize = 4096);
    ~MemoryPool();

    void* allocate();
    void deallocate(void* ptr);

private:
    void allocateNewBlock();
    
private:
    std::size_t m_blockSize;//每个内存块的大小
    size_t m_slotSize;//每个槽的大小
    size_t m_slotCount;//每个内存块的槽数量
    Slot* m_slots;//内存块的槽数组

    Slot* m_freeList;//空闲槽的头指针
    Slot* m_currentSlot;//当前槽的指针
    Slot* m_lastSlot;//最后一个槽的指针

    Slot* m_firstBlock;//内存块的链表
};