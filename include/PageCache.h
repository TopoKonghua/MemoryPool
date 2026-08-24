#include <cstddef>
#include <array>
#include <map>

#define PAGE_ARRAY_SIZE 4096




class PageCache
{
public:
    static PageCache& getInstance()
    {
        static PageCache pageCache;
        return pageCache;
    }

    void* allocateSpan(size_t numPages); // 获取nums个页，返回页起式地址
    void deallocateSpan(void* ptr, size_t numPages); // 归还nums个页，页起始地址为addr

private:
    PageCache() = default;    

    void systemAlloc(size_t numPages); // 向系统申请内存

private:
    struct span
    {
        void* addr;
        size_t nums;
        span* next;
    };
    std::map<size_t, span*> m_freeSpans; // 按页数管理空闲span
    //std::map<void*, span*> m_SpanMap; // 页号到span的映射
};