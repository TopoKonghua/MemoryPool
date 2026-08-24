#pragma once
#include <cstddef>
#include <array>


constexpr size_t BASE_SLOT_SIZE = 8;
constexpr size_t MAX_SLOT_SIZE = 512;
constexpr size_t MEMORY_POOL_NUM = 64;
constexpr size_t FREE_LIST_SIZE = 4096;

#if defined(__GNUC__)
#define ATTRIBUTE_ALWAYS_INLINE __attribute__((always_inline))
#else
#endif

inline ATTRIBUTE_ALWAYS_INLINE void* SLL_Next(void*& t) {
    return *(reinterpret_cast<void**>(t));
}

inline ATTRIBUTE_ALWAYS_INLINE void SLL_SetNext(void*& t, void*& n) {
    *(reinterpret_cast<void**>(t)) = n;
}

inline ATTRIBUTE_ALWAYS_INLINE void SLL_Push(void*& list, void*& element) {
    SLL_SetNext(element, list);
    list = element;
}

inline ATTRIBUTE_ALWAYS_INLINE void* SLL_Pop(void*& list) {
    void* result = list;
    void* next = SLL_Next(list);
    list = next;
    return result;
}

inline ATTRIBUTE_ALWAYS_INLINE void* SLL_Step(void*& t)
{
    return (reinterpret_cast<void**>(t) + 1); 
}
