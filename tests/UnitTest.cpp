#include "MemoryPool.h"

template<typename T>
void print(T* p)
{
    std::cout << typeid(T).name() << "\t";
    std::cout << "内存地址:" << p << "\t";
    std::cout << "值:" << *p << std::endl;
}

int main()
{
    MemoryPool pool(sizeof(int));
    void* p1 = pool.allocate();
    int* p = new(p1) int(100);
    //*p = 100;
    //pool.deallocate(p);
    print(p);
    return 0;
}