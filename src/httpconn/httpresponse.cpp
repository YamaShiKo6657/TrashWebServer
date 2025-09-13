#include"httpresponse.h"
using namespace std;

const unordered_map<string, string> HttpResponse::SUFFIX_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};

const unordered_map<int, string> HttpResponse::CODE_STATUS = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};

const unordered_map<int, string> HttpResponse::CODE_PATH = {
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};

HttpResponse::HttpResponse()
{
    code_=-1;
    srcDir_=path_="";
    isKeepAlive_=false;
    mmFile_=nullptr;
    mmFileStat_={0};
}
HttpResponse::~HttpResponse()
{
    UnmapFile();
}
//初始化
void HttpResponse::Init(const string& srcDir, string& path, bool isKeepAlive, int code)
{
    assert(srcDir!="");
    if(mmFile_)
    {
        UnmapFile();
    }
    srcDir_=srcDir;
    path_=path;
    isKeepAlive_=isKeepAlive;
    code_=code;
    mmFile_ = nullptr; 
    mmFileStat_ = { 0 };
}
//构造http回复报文
void HttpResponse::MakeResponse(Buffer& buff)
{
    if(stat((srcDir_+path_).data(),&mmFileStat_)<0||S_ISDIR(mmFileStat_.st_mode))
    {
        code_=404;
    }
    else if(!(mmFileStat_.st_mode & S_IROTH))
    {
        code_=403;
    }
    else if(code_==-1)
    {
        code_=200;
    }
    ErrorHtml_();
    AddStateLine_(buff);
    AddHeader_(buff);
    AddContent_(buff);
}

char* HttpResponse::File() {
    return mmFile_;
}

size_t HttpResponse::FileLen() const {
    return mmFileStat_.st_size;
}

void HttpResponse::ErrorHtml_()
{
    if(CODE_PATH.count(code_) == 1) {
        path_ = CODE_PATH.find(code_)->second;
        stat((srcDir_ + path_).data(), &mmFileStat_);
    }
}
//添加状态行
void HttpResponse::AddStateLine_(Buffer& buff)
{
    string status;
    if(CODE_STATUS.count(code_)==1)
    {
        status=CODE_STATUS.find(code_)->second;
    }
    else
    {
        code_=400;
        status=CODE_STATUS.find(400)->second;
    }
    buff.Append("HTTP/1.1 " + to_string(code_) + " " + status + "\r\n");
}
//添加头部
void HttpResponse::AddHeader_(Buffer& buff)
{
    buff.Append("Connection: ");
    if(isKeepAlive_)
    {
        buff.Append("keep-alive\r\n");
        buff.Append("keep-alive: max=6, timeout=120\r\n");
    }
    else
    {
        buff.Append("close\r\n");
    }
    buff.Append("Content-type: " + GetFileType_() + "\r\n");
}
//添加内容
void HttpResponse::AddContent_(Buffer& buff)
{
    int srcFd=open((srcDir_+path_).data(),O_RDONLY);
    if(srcFd < 0) 
    { 
        ErrorContent(buff, "File NotFound!");
        return; 
    }
    LOG_DEBUG("file path %s", (srcDir_ + path_).data());
    //将文件映射到内存中
    void* mmRet=mmap(0,mmFileStat_.st_size,PROT_READ,MAP_PRIVATE,srcFd,0);
    if(mmRet == MAP_FAILED)
    {
        ErrorContent(buff, "File NotFound!");
        return; 
    }
    mmFile_ = static_cast<char*>(mmRet);
    close(srcFd);
    buff.Append("Content-length: " + to_string(mmFileStat_.st_size) + "\r\n\r\n");
}
//释放内存映射文件资源
void HttpResponse::UnmapFile()
{
    if(mmFile_)
    {
        munmap(mmFile_, mmFileStat_.st_size);
        mmFile_=nullptr;
    }
    else
    {
        LOG_ERROR("munmap failed: %s", strerror(errno));
    }
}
//判断文件类型
string HttpResponse::GetFileType_()
{
    string::size_type idx=path_.find_last_of('.');
    if(idx=string::npos)
    {
        return "text/plain";
    }
    string suffix=path_.substr(idx);
    if(SUFFIX_TYPE.count(suffix)==1)
    {
        return SUFFIX_TYPE.find(suffix)->second;
    }
    return "text/plain";
}
//错误页
void HttpResponse::ErrorContent(Buffer& buff, string message)
{
    string body;
    string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if(CODE_STATUS.count(code_)==1)
    {
        status=CODE_STATUS.find(code_)->second;
    }
    else
    {
        status = "Bad Request";
    }
    body += to_string(code_) + " : " + status  + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";
    buff.Append("Content-length: " + to_string(body.size()) + "\r\n\r\n");
    buff.Append(body);
}