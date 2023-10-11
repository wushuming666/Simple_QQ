#include <iostream>
#include <pthread.h>

void f()
{
    pthread_exit(0);
}

void* hello(void* arg) {
    std::cout << "Hello from thread!" << std::endl;
    for(int i = 0; i < 5; i ++)
    {
        std::cout << i << std::endl;
        if (i == 2) f();
    }
    return nullptr;
}

int main() {
    pthread_t tid; // 线程标识符

    // 创建一个新线程，将 hello 函数作为线程函数
    if (pthread_create(&tid, nullptr, hello, nullptr) != 0) {
        std::cerr << "Error creating thread" << std::endl;
        return 1;
    }

    // 等待新线程完成
    if (pthread_join(tid, nullptr) != 0) {
        std::cerr << "Error joining thread" << std::endl;
        return 2;
    }

    // 主线程继续执行
    std::cout << "Hello from main!" << std::endl;

    return 0;
}
