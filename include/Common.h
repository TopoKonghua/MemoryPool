#pragma once
#include <cstddef>
#include <array>


constexpr size_t ALIGNMENT = 8;
constexpr size_t MAX_BYTES = 256 * 1024; // 256KB
constexpr size_t FREE_LIST_SIZE = MAX_BYTES / ALIGNMENT; // ALIGNMENT 等于指针void*的大小

#if defined(__GNUC__)
#define ATTRIBUTE_ALWAYS_INLINE __attribute__((always_inline))
#else
#endif

inline ATTRIBUTE_ALWAYS_INLINE void* SLL_Next(void*& t) {
    return *(reinterpret_cast<void**>(t));
}

inline ATTRIBUTE_ALWAYS_INLINE void SLL_SetNext(void*& t, void* n) {
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
