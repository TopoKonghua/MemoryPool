#include "MemoryPool.h"

MemoryPool::MemoryPool(size_t slotSize, size_t blockSize)
{
    m_slotSize = slotSize;
    m_blockSize = blockSize;

    m_slotCount = m_blockSize / m_slotSize;

    m_freeList = nullptr;
    m_currentSlot = nullptr;
    m_lastSlot = nullptr;

}

MemoryPool::~MemoryPool()
{
    for (auto& block : m_blockList)
    {
        operator delete(block);
    }
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
    m_currentSlot += m_slotSize;
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
    m_currentSlot = (Slot*)newBlock;
    m_lastSlot = m_currentSlot + (m_slotCount - 1) * m_slotSize;
    m_blockList.emplace_back(std::move(newBlock));
}