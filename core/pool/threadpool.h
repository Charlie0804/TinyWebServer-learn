#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <assert.h>

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

class ThreadPool
{
   public:
    ThreadPool() = default;
    ThreadPool(ThreadPool&&) = default;
    // 尽量用make_shared代替new，因为通过new再传递给shared_ptr
    // 内存是不连续的，会导致内存碎片化
    explicit ThreadPool(int threadCount = 8) : pool_(std::make_shared<Pool>())
    {
        assert(threadCount > 0);
        for (int i = 0; i < threadCount; i++)
        {
            std::thread(
                [this]()
                {
                    std::unique_lock<std::mutex> locker(pool_->mtx_);
                    while (true)
                    {
                        if (!pool_->tasks.empty())
                        {
                            auto task = std::move(pool_->tasks.front());
                            pool_->tasks.pop();
                            locker.unlock();  // 任务已取出，可以提前解锁
                            task();
                            locker.lock();  // 重新加锁
                        }
                        else if (pool_->isClosed)
                        {
                            break;
                        }
                        else
                        {
                            pool_->cond_.wait(locker);  // 等待任务到来
                        }
                    }
                })
                .detach();
        }
    }

    ~ThreadPool()
    {
        if (pool_)
        {
            std::unique_lock<std::mutex> locker(pool_->mtx_);
            pool_->isClosed = true;
        }
        pool_->cond_.notify_all();
    }

    template <typename T>
    void AddTask(T&& task)
    {
        std::unique_lock<std::mutex> locker(pool_->mtx_);
        pool_->tasks.emplace(std::forward<T>(task));
        pool_->cond_.notify_one();
    }

   private:
    struct Pool
    {
        std::mutex mtx_;
        std::condition_variable cond_;
        bool isClosed;
        std::queue<std::function<void()>> tasks;
    };
    std::shared_ptr<Pool> pool_;
};

#endif  // THREADPOOL_H
