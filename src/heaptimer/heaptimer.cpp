#include "heaptimer.h"
void HeapTimer::SwapNode_(size_t i,size_t j)
{
    assert(i>=0&&i<heap_.size());
    assert(j>=0&&j<heap_.size());
    swap(heap_[i],heap_[j]);
    ref_[heap_[i].id]=i;
    ref_[heap_[j].id]=j;
}
//节点上浮
void HeapTimer::siftup_(size_t i)
{
    assert(i>0&&i<heap_.size());
    size_t parent=(i-1)/2;
    while(parent>=0)
    {
        if(heap_[parent]>heap_[i])
        {
            SwapNode_(parent,i);
            i=parent;
            parent=(i-1)/2;
        }
        else
        {
            break;
        }
    }
}
//节点下沉
bool HeapTimer::siftdown_(size_t i, size_t n)
{
    assert(i<heap_.size()&&i>=0);
    assert(n<=heap_.size()&&n>=0);
    auto index=i,child=(i*2)+1;
    while(child<n)
    {
        if(heap_[child]>heap_[child+1]&&child+1<n)
        {
            child+=1;
        }
        if(heap_[child]<heap_[index])
        {
            SwapNode_(child,index);
            index=child;
            child=index*2+1;
        }
        else
            break;
    }
    return index>i;
}
//删除节点
void HeapTimer::del_(size_t index)
{
    assert(index<heap_.size()&&index>=0);
    size_t n=heap_.size()-1;
    size_t tmp=index;
    if(index<n)
    {
        SwapNode_(tmp,n);
        if(!siftdown_(tmp,n))
        {
            siftup_(tmp);
        }
    }
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}
//调整节点
void HeapTimer::adjust(int id,int newExpires)
{
    assert(!heap_.empty()&&ref_.count(id));
    heap_[ref_[id]].expires=Clock::now()+MS(newExpires);
    siftdown_(ref_[id],heap_.size());
}
//添加或更新定时器任务
void HeapTimer::add(int id,int timeout,const TimeoutCallBack& cb)
{
    assert(id>=0);
    if(ref_.count(id))
    {
        size_t tmp=ref_[id];
        heap_[tmp].expires=Clock::now()+MS(timeout);
        heap_[tmp].cb=cb;
        if(!siftdown_(tmp,heap_.size()))
        {
            siftup_(tmp);
        }
    }
    else
    {
        size_t n=heap_.size();
        ref_[id]=n;
        heap_.push_back({id,Clock::now()+MS(timeout)});
        siftup_(n);
    }
}
// 删除指定id，并触发回调函数
void HeapTimer::doWork(int id)
{
    if(heap_.empty()||ref_.count(id)==0)
    {
        return;
    }
    size_t i=ref_[id];
    auto node=heap_[i];
    node.cb;
    del_(i);
}
//持续检查并执行超时任务
void HeapTimer::tick()
{
    if(heap_.empty())
    {
        return;
    }
    while(heap_.empty())
    {
        TimerNode node=heap_.front();
        if(std::chrono::duration_cast<MS>(node.expires-Clock::now()).count()>0)
        {
            break;
        }
        node.cb;
        pop();
    }
}
//
void HeapTimer::pop()
{
    assert(!heap_.empty());
    del_(0);
}
//
void HeapTimer::clear()
{
    ref_.clear();
    heap_.clear();
}
//计算距离下一个定时任务超时还剩多少毫秒
int HeapTimer::GetNextTick()
{
    tick();
    size_t res=-1;
    if(!heap_.empty())
    {
        res=std::chrono::duration_cast<MS>(heap_.front().expires-Clock::now()).count();
        if(res<0)
        {
            res=0;
        }
    }
    return res;
}
