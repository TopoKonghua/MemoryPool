#include "MemoryPool.h"
#include <cassert>

template<typename T>
void print(T* p)
{
    std::cout << typeid(T).name() << "\t";
    std::cout << "内存地址:" << p << "\t";
    std::cout << "值:" << *p << std::endl;
}

void TestMemoryPoool()
{
    MemoryPool pool(sizeof(int), 40);

    // 分配一个int值初始化为100，释放当前变量内存，再重新申请获取到同一块卡槽
    int* p1 = new(reinterpret_cast<int*>(pool.allocate())) int(100);
    int p1Value = *p1;
    pool.deallocate(reinterpret_cast<void*>(p1));

    int* p2 = new(pool.allocate()) int;
    assert(p1 == p2);
    assert(p1Value != *p2);
    pool.deallocate(p2);

    int* p3[10];
    for(int i = 0; i < 10; ++i)
    {
        p3[i] = new(pool.allocate()) int(i);
    } 
}


int main()
{
    TestMemoryPoool();
    return 0;
}