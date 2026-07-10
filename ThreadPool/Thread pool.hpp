#pragma once

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <iostream>

namespace utils {

    class ThreadPool {
    public:
        /*
         @brief 构造函数，初始化并启动指定数量的线程
         @param threads 线程池中的线程数量，默认为硬件并发线程数
         */
        explicit ThreadPool(size_t threads = std::thread::hardware_concurrency())
            : stop_(false)
        {
            if (threads == 0) {
                threads = 1; // 至少保证有一个工作线程
            }

            for (size_t i = 0; i < threads; ++i) {
                workers_.emplace_back(
                    [this] {
                        for (;;) {
                            std::function<void()> task;

                            {
                                // 加锁保护任务队列
                                std::unique_lock<std::mutex> lock(this->queue_mutex_);

                                // 等待条件：线程池停止 或 任务队列不为空
                                this->condition_.wait(lock,
                                    [this] { return this->stop_ || !this->tasks_.empty(); });

                                // 如果线程池已停止且队列为空，则退出线程
                                if (this->stop_ && this->tasks_.empty()) {
                                    return;
                                }

                                // 取出任务
                                task = std::move(this->tasks_.front());
                                this->tasks_.pop();
                            }

                            // 执行任务，捕获异常以防止工作线程因用户代码异常而崩溃
                            try {
                                task();
                            }
                            catch (const std::exception& e) {
                                // 在工业级应用中，这里通常会接入日志系统
                                std::cerr << "[ThreadPool Exception] " << e.what() << std::endl;
                            }
                            catch (...) {
                                std::cerr << "[ThreadPool Exception] Unknown exception caught." << std::endl;
                            }
                        }
                    }
                );
            }
        }

        // 禁用拷贝构造和赋值操作（线程池不应该被拷贝）
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
        // 禁用移动构造和赋值操作（确保状态安全）
        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;

        /*
         @brief 向线程池提交任务
         @tparam F 函数类型
         @tparam Args 参数类型
         @param f 要执行的函数、Lambda或可调用对象
         @param args 函数参数
         @return std::future 获取任务的返回值或等待任务完成
         */
        template<class F, class... Args>
        [[nodiscard]] auto enqueue(F&& f, Args&&... args)
            -> std::future<typename std::invoke_result_t<F, Args...>>
        {
            // 获取函数返回值类型 (C++17)
            using return_type = typename std::invoke_result_t<F, Args...>;

            // 将任务包装成 std::packaged_task
            // 使用 std::make_shared 是因为 std::function 要求可拷贝，而 packaged_task 是仅可移动的
            auto task = std::make_shared<std::packaged_task<return_type()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...)
            );

            std::future<return_type> res = task->get_future();

            {
                std::unique_lock<std::mutex> lock(queue_mutex_);

                // 提交任务前检查线程池是否已停止
                if (stop_) {
                    throw std::runtime_error("enqueue on stopped ThreadPool");
                }

                // 将任务放入队列
                tasks_.emplace([task]() { (*task)(); });
            }

            // 唤醒一个等待中的工作线程
            condition_.notify_one();
            return res;
        }

        /*
         @brief 析构函数，安全地等待所有任务执行完毕并销毁线程
         */
        ~ThreadPool() {
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                stop_ = true;
            }
            // 唤醒所有线程，让它们检查 stop_ 标志并退出
            condition_.notify_all();

            // 等待所有线程执行完毕
            for (std::thread& worker : workers_) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
        }

    private:
        std::vector<std::thread> workers_;          ///< 工作线程数组
        std::queue<std::function<void()>> tasks_;   ///< 任务队列

        std::mutex queue_mutex_;                    ///< 互斥锁，保护任务队列和停止标志
        std::condition_variable condition_;         ///< 条件变量，用于阻塞和唤醒线程
        bool stop_;                                 ///< 线程池停止标志
    };

} // namespace utils
