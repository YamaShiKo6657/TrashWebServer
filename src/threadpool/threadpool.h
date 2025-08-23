#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <assert.h>

class ThreadPool
{
public:
    ThreadPool() = default;
    ThreadPool(ThreadPool&&) = default;
    explicit ThreadPool(int threadCount = 8);
    ~ThreadPool();
    template<typename T> void AddTask(T&& task);

private:
    struct pool
    {
        std::mutex mtx_;
        std::condition_variable cond_;
        bool isClosed;
        std::queue<std::function<void()>> tasks;
    };
    std::shared_ptr<pool> pool_;
};
explicit ThreadPool::ThreadPool(int threadcount):pool_(std::make_shared<pool>())
{
    assert(threadcount>0);
    for(int i=0;i<threadcount;i++)
    {
        std::thread
        (
            [this]()
                {
                    std::unique_lock<std::mutex> locker(pool_->mtx_);
                    while(true)
                    {
                        if(!pool_->tasks.empty())
                        {
                        auto task=move(pool_->tasks.front());
                        pool_->tasks.pop();
                        locker.unlock();
                        task();
                        locker.lock();
                        }
                        else if(pool_->isClosed==true)
                        {
                            break;
                        }
                        else
                        {
                            pool_->cond_.wait(locker);
                        }
                    }
                }
        ).detach();//通过 detach() 让线程在后台运行，不阻塞主线程。
    }
}
ThreadPool::~ThreadPool()
{
    if(pool_)
    {
        std::unique_lock<std::mutex> locker(pool_->mtx_);
        pool_->isClosed=true;
    }
    pool_->cond_.notify_all();
}
template<typename T>
void ThreadPool::AddTask(T&& task)
{
    std::unique_lock<std::mutex> locker(pool_->mtx_);
    pool_->tasks.emplace(std::forward<T>(task));
    pool_->cond_.notify_one();
}


#endif