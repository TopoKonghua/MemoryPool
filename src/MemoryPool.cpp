#include "MemoryPool.h"

MemoryPool::MemoryPool(size_t blockSize)
{
    m_slotSize = BASE_SLOT_SIZE;
    m_blockSize = blockSize;
    m_slotCount = 0;

    m_currentSlot = nullptr;
    m_lastSlot = nullptr;
    m_firstBlock = nullptr;
}

MemoryPool::~MemoryPool()
{
    Slot* currentBlock = m_firstBlock;
    while (currentBlock)
    {
        m_firstBlock = m_firstBlock->next;
        operator delete(reinterpret_cast<void*>(currentBlock));
        currentBlock = m_firstBlock;
    }
}

void MemoryPool::init(size_t slotSize)
{
    m_slotSize = slotSize;
    m_slotCount = (m_blockSize - sizeof(Slot)) / m_slotSize;
    m_freeList.store(nullptr, std::memory_order_relaxed);
}

void* MemoryPool::allocate()
{
    Slot* slot = popFreeList();
    if (slot)
        return slot;
   

    void* retSlot;
    {  
        std::lock_guard<std::mutex> lock(m_mutexForBlock);
        if (m_currentSlot == m_lastSlot) // 已用完内存块，重新申请一块
        {
            allocateNewBlock();
        }
        retSlot = m_currentSlot;
        m_currentSlot += m_slotSize / sizeof(Slot);
    }
    return retSlot;
}

void MemoryPool::deallocate(void* ptr)
{
    if (!ptr) return;
    Slot* slot = reinterpret_cast<Slot*>(ptr);
    pushFreeList(slot);
}

void MemoryPool::pushFreeList(Slot *slot)
{
    //std::lock_guard<std::mutex> lock(m_mutexForFreeList);
    while (true)
    {
        Slot* oldHead = m_freeList.load(std::memory_order_relaxed);
        slot->next.store(oldHead, std::memory_order_relaxed);
        if (m_freeList.compare_exchange_weak(oldHead, slot, std::memory_order_release, 
            std::memory_order_relaxed))
            return;
    }
}

Slot *MemoryPool::popFreeList()
{
    std::lock_guard<std::mutex> lock(m_mutexForFreeList);
    while (true)
    {
        Slot* slot = m_freeList.load(std::memory_order_relaxed);
        if (slot == nullptr)
            return nullptr;

        Slot* next = nullptr;
        try
        {
            next = slot->next.load(std::memory_order_relaxed);
        }
        catch(...)
        {
            continue;
        }

        if (m_freeList && m_freeList.compare_exchange_weak(slot, next, std::memory_order_acquire,
            std::memory_order_relaxed))
        {
            return slot;
        }
    }
}

void MemoryPool::allocateNewBlock()
{
    void* newBlock = operator new(m_blockSize);
    
    Slot* s0 = (Slot*)newBlock;
    s0->next = m_firstBlock;
    m_firstBlock = s0;

    m_currentSlot = s0 + 1;
    m_lastSlot = m_currentSlot + m_slotCount;
}

void HashBucket::initMemoryPool()
{
    for (int i = 0; i < MEMORY_POOL_NUM; ++i)
        getMemoryPool(i).init((i + 1) * BASE_SLOT_SIZE);
}

MemoryPool& HashBucket::getMemoryPool(size_t index)
{
    static MemoryPool memoryPool[MEMORY_POOL_NUM];
    return memoryPool[index];
}