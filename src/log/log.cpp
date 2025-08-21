#include"log.h"

Log::Log()
{
    fp_=nullptr;
    writeThread_=nullptr;
    isAsync_=false;
    deque_=nullptr;
    lineCount_=0;
    toDay_=0;
}
Log::~Log()
{
    while(!deque_->empty())
    {
        flush();
    }
    deque_->Close();
    writeThread_->join();
    if(fp_)
    {
        lock_guard<mutex> locker(mtx_);
        flush();
        fclose(fp_);
    }
}
//刷新缓冲区,唤醒消费者
void Log::flush()
{
    if(isAsync_)
    {
        deque_->flush();
    }
    fflush(fp_);
}
//局部变量法
Log* Log::Instance()
{
    static Log log;
    return &log;
}
//异步日志的写线程函数
void Log::FlushLogThread()
{
    Log::Instance()->AsyncWrite_();
}
//写线程的执行函数
void Log::AsyncWrite_()
{
    string str="";
    while(deque_->pop(str))
    {
        lock_guard<mutex> locker(mtx_);
        fputs(str.c_str(),fp_);
    }
}
//初始化实例
void Log::init(int level, const char* path, const char* suffix, int maxQueCapacity)
{
    isOpen_=true;
    level_=level;
    path_=path;
    suffix_=suffix;
    //异步模式初始化
    if(maxQueCapacity)
    {
        isAsync_=true;
        if(!deque_)
        {
            unique_ptr<BlockQueue<string>> newQue(new BlockQueue<string>);
            deque_=move(newQue);
            unique_ptr<thread> newThread(new thread(FlushLogThread));
            writeThread_=move(newThread);
        }
    }
    else
    {
        isAsync_=false;
    }
    //日志文件初始化
    lineCount_=0;
    time_t timer=time(nullptr);
    struct tm* systime=localtime(&timer);
    char filename[LOG_NAME_LEN]={0};
    snprintf(filename,LOG_NAME_LEN-1,"%s/%04d_%02d_%02d%s",
    path_,systime->tm_year+1900,systime->tm_mon+1,systime->tm_mday,suffix_);
    //当前日期
    toDay_=systime->tm_mday;
    //独立作用域
    {
        lock_guard<mutex> locker(mtx_);
        buff_.RetrieveAll();
        if(fp_)
        {
            flush();
            fclose(fp_);
        }
        fp_=fopen(filename,"a");
        if(fp_==nullptr)
        {
            mkdir(filename,0777);
            fp_=fopen(filename,"a");
        }
        assert(fp_!=nullptr);
    }
}
