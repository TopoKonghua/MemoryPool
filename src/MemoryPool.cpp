#include "MemoryPool.h"

MemoryPool::MemoryPool(size_t slotSize, size_t blockSize)
{
    m_slotSize = slotSize;
    m_blockSize = blockSize;

    m_slotCount = m_blockSize / m_slotSize;

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
    if (m_currentSlot == m_lastSlot)
    {
        allocateNewBlock();
    }
    void* retSlot = m_currentSlot;
    m_currentSlot += m_slotSize;
    return retSlot;
}

void MemoryPool::deallocate(void* ptr)
{

}
void MemoryPool::allocateNewBlock()
{
    void* newBlock = operator new(m_blockSize);
    m_currentSlot = (Slot*)newBlock;
    m_lastSlot = m_currentSlot + (m_slotCount - 1) * m_slotSize;
    m_blockList.emplace_back(std::move(newBlock));
}