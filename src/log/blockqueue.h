# ifndef BLOCKQUEUE_H
# define BLOCKQUEUE_H

#include <deque>
#include <condition_variable>
#include <mutex>
#include <sys/time.h>
using namespace std;

template<typename T>
class BlockQueue {
public:
    explicit BlockQueue(size_t maxsize = 1000);
    ~BlockQueue();
    bool empty();
    bool full();
    void push_back(const T& item);
    void push_front(const T& item); 
    bool pop(T& item);  // 弹出的任务放入item
    bool pop(T& item, int timeout);  // 等待时间
    void clear();
    T front();
    T back();
    size_t capacity();
    size_t size();

    void flush();
    void Close();

private:
    deque<T> deq_;                      // 底层数据结构
    mutex mtx_;                         // 锁
    bool isClose_;                      // 关闭标志
    size_t capacity_;                   // 容量
    condition_variable condConsumer_;   // 消费者条件变量
    condition_variable condProducer_;   // 生产者条件变量
};
template <typename T>
//成员初始化列表
BlockQueue<T>:: BlockQueue(size_t maxsize):capacity(maxsize),isClose_(false)
{
    assert(maxsize>0);
}
template<typename T>
BlockQueue<T>::~BlockQueue()
{
    Close();
}
//关闭阻塞队列
template<typename T>
void BlockQueue<T>::Close()
{
    clear();
    isClose_=true;
    condConsumer_.notify_all();
    condProducer_.notify_all();
}
template<typename T>
void BlockQueue<T>::clear()
{
    lock_guard<mutex> lock(mtx_);
    deq_.clear();
}
//检查队列是否为空
template<typename T>
bool BlockQueue<T>::empty()
{
    lock_guard<mutex> lock(mtx_);
    return deq_.empty();
}
//检查队列是否为满
template<typename T>
bool BlockQueue<T>::full()
{
    lock_guard<mutex> lock(mtx_);
    return deq_.size()>=capacity_;
}
//向阻塞队列增加元素
template<typename T>
void BlockQueue<T>::push_back(const T& item)
{
    unique_lock<mutex> lock(mtx_);
    while(full())
    {
        condProducer_.wait(lock);
    }
    deq_.push_back(item);
    condConsumer_.notify_one();
}
template<typename T>
void BlockQueue<T>::push_front(const T& item)
{
    unique_lock<mutex> lock(mtx_);
    while(full())
    {
        condProducer_.wait(lock);
    }
    deq_.push_front(item);
    condConsumer_.notify_one();
}
//从队列中取出数据
template<typename T>
bool BlockQueue<T>::pop(T& item)
{
    unique_lock<mutex> lock(mtx_)
    while(empty())
    {
        condConsumer_.wait(lock);
    }
    item=deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}


#endif