# C++17 工业级线程池 ThreadPool
Header-only 单头文件并发线程池，无内存泄漏、无逻辑漏洞，适配C++17及以上标准。

## 核心特性
1. Header-Only，仅引入头文件即可使用，无需编译库
2. [[nodiscard]] 标记任务返回值，杜绝遗漏异常
3. 内置try-catch，任务报错不会摧毁工作线程
4. RAII自动管理线程，销毁时全部join，无僵尸线程
5. 禁用拷贝/移动构造，防止锁竞争崩溃
6. 条件变量双重判断，解决虚假唤醒问题

## 完整使用示例
```cpp
#include <iostream>
#include "ThreadPool.hpp"

int main() {
    // 初始化4线程池
    utils::ThreadPool pool(4);

    // 提交带返回值任务
    auto res = pool.enqueue([](int x, int y) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return x + y;
    }, 10, 20);

    // 提交无返回后台任务
    pool.enqueue([] {
        std::cout << "子线程执行任务\n";
    });

    // 阻塞获取计算结果
    std::cout << "10 + 20 = " << res.get() << std::endl;
    return 0;
}
