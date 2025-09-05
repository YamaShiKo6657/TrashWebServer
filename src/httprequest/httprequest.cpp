#include"httprequest.h"

using namespace std;

const unordered_set<string> HttpRequest::DEFAULT_HTML{"/index", "/register", "/login",
             "/welcome", "/video", "/picture",};
const unordered_map<string,int> HttpRequest::DEFAULT_HTML_TAG{{"/register.html",0},{"/login.html",1}};

void HttpRequest::Init()
{
    method_=path_=version_=body_="";
    state_=REQUEST_LINE;
    header_.clear();
    post_.clear();
}
//判断是否Keep-Alive
bool HttpRequest::IsKeepAlive() const
{
    if(header_.count("Connection")==1)
    {
        return header_.find("Connection")->second=="keep-alive"&&version_=="1.1";
    }
    return false;
}
//解析处理
bool HttpRequest::parse(Buffer& buff)
{
    const char CRLF[]="\r\n";
    if(buff.ReadableBytes()<=0)
    {
        return false;
    }
    while(buff.ReadableBytes()&&state_!=FINISH)
    {
        const char* lineEnd=search(buff.Peek(),buff.BeginWriteConst(),CRLF,CRLF+2);
        string line(buff.Peek(),lineEnd);
        switch (state_)
        {
        case REQUEST_LINE:
            if(!ParseRequestLine_(line))
            {
                return false;
            }
            ParsePath_();
            break;
        case HEADERS:
            ParseHeader_(line);
            if(buff.ReadableBytes()<=2)
            {
                state_=FINISH;
            }
            break;
        case BODY:
            ParseBody_(line);
            break;
        default:
            break;
        }
        if(lineEnd==buff.BeginWrite())
        {
            break;
        }
        buff.RetrieveUntil(lineEnd+2);
    }
    LOG_DEBUG("[%s],[%s],[%s]",method_.c_str(),path_.c_str(),version_.c_str());
    return true;
}
// 解析路径
void HttpRequest::ParsePath_()
{
    if(path_=="/")
    {
        path_="/index.html";
    }
    else
    {
        for(auto& item:DEFAULT_HTML)
        {
            if(path_==item)
            {
                path_+=".html";
                break;
            }
        }
    }
}

bool HttpRequest::ParseRequestLine_(const string& line)
{
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    smatch submatch;
    if(regex_match(line,submatch,patten))
    {
        method_=submatch[1];
        path_=submatch[2];
        version_=submatch[3];
        state_=HEADERS;
        return true;
    }
    LOG_ERROR("RequestLine Error");
    return false;
}

