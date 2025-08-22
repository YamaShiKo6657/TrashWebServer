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
//实现写日志函数
void Log::write(int level,const char* format,...)
{
    //获取当前系统时间
    struct timeval now={0,0};
    gettimeofday(&now,nullptr);
    time_t tSec=now.tv_sec;
    struct tm *systime=localtime(&tSec);
    struct tm t=*systime;

    va_list valist;
    if(toDay_=t.tm_mday||(lineCount_&&(lineCount_%MAX_LINES==0)))
    {
        char newfile[LOG_NAME_LEN];
        char tail[36]={0};
        snprintf(tail,36,"%04d_%02d_%02d",t.tm_year+1900,t.tm_mon+1,t.tm_mday);
        if(toDay_!=t.tm_mday)
        {
            snprintf(newfile,LOG_NAME_LEN-72,"%s/%s%s",path_,tail,suffix_);
            toDay_=t.tm_mday;
            lineCount_=0;
        }
        else
        {
            snprintf(newfile,LOG_NAME_LEN,"%s/%s-%d%s",path_,tail,lineCount_%MAX_LINES,suffix_);
        }
        {
        unique_lock<mutex> locker(mtx_);
        flush();
        fclose(fp_);
        fp_ = fopen(newfile, "a");
        assert(fp_ != nullptr);
        }
    }
    {
        unique_lock<mutex> locker(mtx_);
        lineCount_++;
        int n=snprintf(buff_.BeginWrite(),128,"%d-%02d-%02d %02d:%02d:%02d.%06ld",t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
        buff_.HasWritten(n);
        AppendLogLevelTitle_(level);

        va_start(valist,format);
        int m=vsnprintf(buff_.BeginWrite(),buff_.WritableBytes(),format,valist);
        va_end(valist);
        buff_.HasWritten(m);
        buff_.Append("\n\0",2);

        if(isAsync_&&deque_&&!deque_->full())//异步模式
        {
            deque_->push_back(buff_.RetrieveAllToStr());
        }
        else
        {
            fputs(buff_.Peek(),fp_);//同步模式
        }
        buff_.RetrieveAll();
    }

}
// 添加日志等级
void Log::AppendLogLevelTitle_(int level)
{
    switch(level)
    {
        case 0:
        buff_.Append("[debug]: ", 9);
        break;
        case 1:
        buff_.Append("[info] : ", 9);
        break;
        case 2:
        buff_.Append("[warn] : ", 9);
        break;
        case 3:
        buff_.Append("[error]: ", 9);
        break;
        default:
        buff_.Append("[info] : ", 9);
        break;
    }
}
//获取等级
int Log::GetLevel()
{
    lock_guard<mutex> locker(mtx_);
    return level_;
}
//设置等级
void Log::SetLevel(int level)
{
    lock_guard<mutex> locker(mtx_);
    level_=level;
}