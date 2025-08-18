#include"buffer.h"

Buffer::Buffer(int InitBufferSize):buffer_(InitBufferSize),readPos_(0),writePos_(0){}
//返回可写区域
size_t Buffer::WritableBytes() const
{
    return buffer_.size()-writePos_;
}
//返回可读区域
size_t Buffer::ReadableBytes() const
{
    return writePos_-readPos_;
}
//返回预留区域
size_t Buffer::PrependableBytes() const
{
    return readPos_;
}
//返回在读位置指针
const char* Buffer::Peek() const
{
    return &buffer_[readPos_];
}
//确保长度小于可写空间，若大于则扩容
void Buffer::EnsureWriteable(size_t len)
{
    if(len>WritableBytes())
    {
        MakeSpace_(len);
    }
    assert(len<=WritableBytes());
}
//移动写下标,在append函数中使用
void Buffer::HasWritten(size_t len)
{
    writePos_+=len;
}
//移动读下标
void Buffer::Retrieve(size_t len)
{
    readPos_+=len;
}
//移动读下标到末尾
void Buffer::RetrieveUntil(const char* end)
{
    assert(Peek()<=end);
    Retrieve(end-Peek());
}
//将容器清空
void Buffer::RetrieveAll()
{
    bzero(&buffer_[0],buffer_.size());
    readPos_=0;
    writePos_=0;
}
//将可读区域写入字符串中并清空容器
std::string Buffer::RetrieveAllToStr()
{
    std::string str(Peek(),ReadableBytes());
    RetrieveAll();
    return str;
}
//返回可写区域指针，不可更改
const char* Buffer::BeginWriteConst() const
{
    return &buffer_[writePos_];
}
//返回可写区域指针
char* Buffer::BeginWrite()
{
    return &buffer_[writePos_];
}
//向缓冲区追加数据
void Buffer::Append(const char* str,size_t len)
{
    assert(str);
    EnsureWriteable(len);
    std::copy(str,str+len,BeginWrite());
    HasWritten(len);
}
//向缓冲区追加字符串
void Buffer::Append(const std::string &str)
{
    Append(str.c_str(),str.size());
}
//向缓冲区追加无符号（二进制）数据
void Buffer::Append(const void* data,size_t len)
{
    Append(static_cast<const char*>(data),len);
}
//向缓冲区追加其他buffer的可读部分
void Buffer::Append(const Buffer &buff)
{
    Append(buff.Peek(),buff.ReadableBytes());
}
// 将fd的内容读到缓冲区
ssize_t Buffer::ReadFd(int fd,int* Errno)
{
    char buff[65535];
    size_t writable=WritableBytes();
    struct iovec iov[2];
    iov[0].iov_base=BeginWrite();
    iov[0].iov_len=writable;
    iov[1].iov_base=buff;
    iov[1].iov_len=sizeof(buff);
    ssize_t len=readv(fd,iov,2);
    if(len<0)
    {
        *Errno=errno;
    }
    else if(static_cast<size_t>(len)<=writable)
    {
        writePos_+=len;
    }
    else
    {
        Append(buff,static_cast<size_t>(len-writable));
    }
    return len;
}
//将缓冲区可读内容读入fd中
ssize_t Buffer::WriteFd(int fd,int* Errno)
{
    ssize_t len=write(fd,Peek(),ReadableBytes());
    if(len<0)
    {
        *Errno=errno;
    }
    Retrieve(len);
    return len;
}
//返回buffer地址
char* Buffer::BeginPtr_()
{
    return &buffer_[0];
}
const char* Buffer::BeginPtr_() const
{
    return &buffer_[0];
}
//拓展空间
void Buffer::MakeSpace_(size_t len)
{
    if(WritableBytes()+PrependableBytes()<len)
    {
        buffer_.resize(writePos_+len+1);
    }
    else
    {
        size_t readable=ReadableBytes();
        std::copy(BeginPtr_()+readPos_,BeginPtr_()+writePos_,BeginPtr_());
        readPos_=0;
        writePos_=readable;
        assert(readable==ReadableBytes());
    }
}