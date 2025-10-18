#include "webserver.h"

using namespace std;

WebServer::WebServer(
            int port, int trigMode, int timeoutMS, bool OptLinger,
            int sqlPort, const char* sqlUser, const  char* sqlPwd,
            const char* dbName, int connPoolNum, int threadNum,
            bool openLog, int logLevel, int logQueSize):
            port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMS), isClose_(false),
            timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller())
{
    srcDir_=getcwd(nullptr,256);
    assert(srcDir_);
    strcat(srcDir_,"/resources/");
    HttpConn::userCount=0;
    HttpConn::srcDir = srcDir_;
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);
    InitEventMode_(trigMode);
    if(!InitSocket_())
    {
        isClose_=true;
    }
    if(openLog)
    {
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
        if(isClose_) { LOG_ERROR("========== Server init error!=========="); }
        else {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d, OpenLinger: %s", port_, OptLinger? "true":"false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                            (listenEvent_ & EPOLLET ? "ET": "LT"),
                            (connEvent_ & EPOLLET ? "ET": "LT"));
            LOG_INFO("LogSys level: %d", logLevel);
            LOG_INFO("srcDir: %s", HttpConn::srcDir);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }

}
//
WebServer::~WebServer()
{
    close(listenFd_);
    isClose_=true;
    free(srcDir_);
    SqlConnPool::Instance()->ClosePool();
}
//
void WebServer::InitEventMode_(int trigMode)
{
    listenEvent_=EPOLLRDHUP;
    connEvent_=EPOLLONESHOT|EPOLLRDHUP;
    switch(trigMode)
    {
    case 0:
        break;
    case 1:
        connEvent_|=EPOLLET;
        break;
    case 2:
        listenEvent_|=EPOLLET;
        break;
    case 3:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    default:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    HttpConn::isET=(connEvent_&EPOLLET);
}
//
void WebServer::Start()
{
    int timeMS=-1;
    if(!isClose_) 
    { 
        LOG_INFO("========== Server start ==========");
    }
    while(!isClose_)
    {
        if(timeoutMS_>0)
        {
            timeMS=timer_->GetNextTick();
        }
        int eventCnt=epoller_->Wait();
        for(int i=0;i<eventCnt;i++)
        {
            int fd=epoller_->GetEventFd(i);
            uint32_t event=epoller_->GetEvents(i);
            if(fd=listenFd_)
            {
                DealListen_();
            }
            else if(event&EPOLLRDHUP|EPOLLHUP|EPOLLERR)
            {
                assert(users_.count(fd)>0);
                CloseConn_(&users_[fd]);
            }
            else if(event&EPOLLIN)
            {
                assert(users_.count(fd)>0);
                DealRead_(&users_[fd]);
            }
            else if(event&EPOLLOUT)
            {
                assert(users_.count(fd));
                DealWrite_(&users_[fd]);
            }
            else
            {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}
//
void WebServer::SendError_(int fd,const char* info)
{
    assert(fd>0);
    int ret=send(fd,info,strlen(info),0);
    if(ret < 0) 
    {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}
//
void WebServer::CloseConn_(HttpConn* client)
{
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();
}
//处理新客户端连接
void WebServer::AddClient_(int fd,sockaddr_in addr)
{
    assert(fd>0);
    users_[fd].init(fd,addr);
    if(timeoutMS_>0)
    {
        timer_->add(fd,timeoutMS_,std::bind(&WebServer::CloseConn_,this,&users_[fd]));
    }
    epoller_->AddFd(fd,EPOLLIN|connEvent_);
    SetFdNonblock(fd);
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());
}
// 处理监听套接字，主要逻辑是accept新的套接字，并加入timer和epoller中
void WebServer::DealListen_()
{
    struct sockaddr_in addr;
    socklen_t len=sizeof(addr);
    do
    {
        int fd=accept(listenFd_,(struct sockaddr*)&addr,&len);
        if(fd<0)
        {
            return;
        }
        else if(HttpConn::userCount>MAX_FD)
        {
            SendError_(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        AddClient_(fd,addr);
    } while (listenEvent_&EPOLLET);
}
//处理读事件
void WebServer::DealRead_(HttpConn* client)
{
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnRead_,this,client));
}
//处理写事件
void WebServer::DealWrite_(HttpConn* client)
{
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_,this,client));
}
//更新连接对应的fd在定时器中的超时时间，防止活跃连接被误关闭。
void WebServer::ExtentTime_(HttpConn* client)
{
    assert(client);
    if(timeoutMS_>0)
    {
        timer_->adjust(client->GetFd(),timeoutMS_);
    }
}
//
void WebServer::OnRead_(HttpConn* client)
{
    assert(client);
    int ret=-1;
    int readErrno=0;
    ret=client->read(&readErrno);
    if(ret<=0&&readErrno!=EAGAIN)
    {
        CloseConn_(client);
        return;
    }
    OnProcess(client);
}
//处理读（请求）数据的函数
void WebServer::OnProcess(HttpConn* client)
{
    if(client->process())
    {
        epoller_->ModFd(client->GetFd(),connEvent_|EPOLLOUT);
    }
    else
    {
        epoller_->ModFd(client->GetFd(),connEvent_|EPOLLIN);
    }
}
//
void WebServer::OnWrite_(HttpConn* client)
{
    assert(client);
    int ret=-1;
    int writeErrno=0;
    ret=client->write(&writeErrno);
    if(client->ToWriteBytes()==0)
    {
        if(client->IsKeepAlive())
        {
            epoller_->ModFd(client->GetFd(),connEvent_|EPOLLIN);
            return;
        }
    }
    else if(ret<0)
    {
        if(writeErrno==EPOLLIN)
        {
            epoller_->ModFd(client->GetFd(),connEvent_|EPOLLIN);
            return;
        }
    }
    CloseConn_(client);
}
//服务器监听套接字的创建、配置、绑定及事件注册
bool WebServer::InitSocket_()
{
    int ret;
    struct sockaddr_in addr;
    if(port_ > 65535 || port_ < 1024) 
    {
        LOG_ERROR("Port:%d error!",  port_);
        return false;
    }
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    addr.sin_port=htons(port_);
    // 优雅关闭
    {
        struct linger optLinger;
        if(openLinger_)
        {
            optLinger.l_linger=1;
            optLinger.l_onoff=1;
        }
        listenFd_=socket(AF_INET,SOCK_STREAM,0);
        if(listenFd_ < 0) 
        {
        LOG_ERROR("Create socket error!", port_);
        return false;
        }
        ret=setsockopt(listenFd_,SOL_SOCKET,SO_LINGER,&optLinger,sizeof(optLinger));
        if(ret < 0) 
        {
        close(listenFd_);
        LOG_ERROR("Init linger error!", port_);
        return false;
        }
        
    }

    ret=bind(listenFd_,(struct sockaddr*)&addr,sizeof(addr));
    if(ret < 0) 
    {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }
    ret=listen(listenFd_,6);
    if(ret < 0) 
    {
        LOG_ERROR("Listen port:%d error!", port_);
        close(listenFd_);
        return false;
    }
    ret=epoller_->AddFd(listenFd_,listenEvent_|EPOLLIN);
    if(ret == 0) 
    {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    }
    SetFdNonblock(listenFd_);  
    LOG_INFO("Server port:%d", port_);
    return true;
}
int WebServer::SetFdNonblock(int fd) 
{
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}
