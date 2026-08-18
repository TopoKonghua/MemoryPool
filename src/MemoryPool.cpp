#include "MemoryPool.h"

MemoryPool::MemoryPool(size_t blockSize)
{
    m_slotSize = BASE_SLOT_SIZE;
    m_blockSize = blockSize;
    m_slotCount = 0;

    m_freeList = nullptr;
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
}

void* MemoryPool::allocate()
{
    if (m_freeList) // 优先使用回收后的卡槽
    {
        void* firstSlot = m_freeList;
        m_freeList = m_freeList->next;
        return std::move(firstSlot);    
    }

    if (m_currentSlot == m_lastSlot) // 已用完内存块，重新申请一块
    {
        allocateNewBlock();
    }
    void* retSlot = m_currentSlot;
    m_currentSlot += m_slotSize / sizeof(Slot);
    return retSlot;
}

void MemoryPool::deallocate(void* ptr)
{
    Slot* slot = reinterpret_cast<Slot*>(ptr);
    slot->next = m_freeList;
    m_freeList = slot;
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