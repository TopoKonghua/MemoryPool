#include <cstddef>
#include <atomic>
#include <vector>
#include <array>

#define SPAN_ARRAY_SIZE 4096
#define FREE_LIST_SIZE 4096

// 存储元数据
struct SpanTracker
{
    std::atomic<void*> spanAddr{nullptr}; // 存储页的地址
    std::atomic<size_t> numPages; // 页数
    std::atomic<size_t> blockCount; // 分配出的块数
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
    void returnRange(size_t index, void* ptr); // 归还 index 对应的内存链表

private:
    CentralCache() = default;

    void* fetchFromPageCache(size_t size); // 从页缓存获取size页

    // 更新span的空闲计数并检查是否可归还

private:
    std::array<SpanTracker, SPAN_ARRAY_SIZE> m_centralSpan;
    
    std::array<std::atomic_flag, FREE_LIST_SIZE> m_centralFreeFlag;
    std::array<std::atomic<void*>, FREE_LIST_SIZE> m_centralFreeList;
};