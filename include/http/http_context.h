#pragma once
#include "http_request.h"
#include"buffer.h"
#include"http_utility.h"


typedef enum
{
    RECV_HTTP_ERROR,
    RECV_HTTP_LINE,
    RECV_HTTP_HEAD,
    RECV_HTTP_BODY,
    RECV_HTTP_OVER
} HttpRecvStatus;

#define MAX_LINE 8192 // 最大行长度
class Httpcontext // 请求更新上下文，接收的数据不是一条完整的数据
{
private:
    int _resp_status;            // 响应状态码
    HttpRecvStatus _recv_status; // 接收及解析状态
    HttpRequest _request;        // 已经解析得到的请求信息
private:
    bool RecvHttpLine(Buffer *buff)
    {
        if(_recv_status!=RECV_HTTP_LINE)
        {
            return false;
        }
        std::string line=buff->Getline();
        if(line.size()==0)
        {   //如果buff中的可读数据比最大长度还大但还是不足一行，那一定是有问题的
            if(buff->ReadAbleSize()>MAX_LINE)
            {
                _recv_status=RECV_HTTP_ERROR;
                _resp_status=414;//URI Too Long
                return false;
            }
            //数据不够再等等
            return true;
        }
        if(line.size()>MAX_LINE)
        {
            _recv_status=RECV_HTTP_ERROR;
            _resp_status=414;//URI Too Long
            return false;
        }
        bool ret=ParseHttpLine(line);
        if(ret==false)
        {
            return false;
        }
        //首行处理完毕，进入头部处理
        _recv_status=RECV_HTTP_HEAD;
        return true;
     }
    bool RecvHttpHead(Buffer *buff)
    {
        if(_recv_status!=RECV_HTTP_HEAD)
        {
            return false;
        }
        while(1)
        {
            std::string line=buff->Getline();
            if(line.size()==0)
            {
                if(buff->ReadAbleSize()>MAX_LINE)
                {
                    _recv_status=RECV_HTTP_ERROR;
                    _resp_status=414;//URI Too Long
                    return false;
                }
                return true;
                
            }
            if(line.size()>MAX_LINE)//数据太长处理不了
            {
                _recv_status=RECV_HTTP_ERROR;
                _resp_status=414;//URI Too Long
                return false;
            }
            if(line=="\n"||line=="\r\n")//读到空行，头部结束
            {
                break;
            }
             bool ret=ParseHttpHead(line);
             if(ret==false)
             {
                return false;
             }
        }
        _recv_status=RECV_HTTP_BODY;
        return true;
    }
    bool RecvHttpBody(Buffer *buff)
    {
        if(_recv_status!=RECV_HTTP_BODY)
        {
            return false;
        }
        //获取正文长度
        size_t len=_request.ContentLength();
        if(len==0)
        {
            _recv_status=RECV_HTTP_OVER;
            return true;
        }
        //获取实际需要读取的长度
        int real_len=len-_request._body.size();
        //缓冲区内的数据很多，就读完
        if(buff->ReadAbleSize()>= real_len)
        {
            _request._body.append(buff->GetReadPos(),real_len);
            buff->MoveReadPos(real_len);
            _recv_status=RECV_HTTP_OVER;
            return true;
        }
        //缓冲区的数据不满足需要读取的长度，就有多少读多少，等待下次
        _request._body.append(buff->GetReadPos(),buff->ReadAbleSize());
        buff->MoveReadPos(buff->ReadAbleSize());
        return true;
    }   
    bool ParseHttpLine(const std::string& line)
    {   //正则匹配
        std::smatch matches;
        std::regex e("(GET|POST|PUT|HEAD|DELETE) ([^?]*)(?:\\?(.*))? (HTTP/1\\.[01])(?:\n|\r\n)?",std::regex::icase);
        bool ret= std::regex_match(line,matches,e);
        if(ret==false)
        {
            _recv_status=RECV_HTTP_ERROR;
            _resp_status=400;//Bad Request
            return false;
        }
        _request._method=matches[1];//请求方法
        _request._path=Util::UrlDecode(matches[2],false);//请求路径
        _request._version=matches[4];//版本
        //提取查询字符串
        std::vector<std::string> query_string_arr;
        std::string query_str=matches[3];
        //查询字符串的格式是key1=value1&key2=value2按&将其分割
        Util::Split(query_str,"&",&query_string_arr);
        //针对各个子串 以 = 分割得到key和value 进行解码后插入
        for(auto& str:query_string_arr)
        {
            size_t pos=str.find("=");
            if(pos==std::string::npos)
            {
                _recv_status=RECV_HTTP_ERROR;
                _resp_status=400;//Bad Request
                return false;
            }
            std::string key=Util::UrlDecode(str.substr(0,pos),true);
            std::string value=Util::UrlDecode(str.substr(pos+1),true);
            _request.SetParam(key,value);
        }
        return true;
    }
    bool ParseHttpHead( std::string& head)
    {
        //末尾是\r或\n去掉
        if(head.back()=='\r')
        {  
            head.pop_back();
        }
        if(head.back()=='\n')
        {
            head.pop_back();
        }
        size_t pos=head.find(": ");
        if(pos==std::string::npos)
        {
            _recv_status=RECV_HTTP_ERROR;
            _resp_status=400;//Bad Request
            return false;
        }
        std::string key=head.substr(0,pos);
        std::string value=head.substr(pos+2);
        _request.SetHeader(key,value);
        return true;
    }
public:
    Httpcontext():_resp_status(200),_recv_status(RECV_HTTP_LINE){}
    void Reset()
    {
        _resp_status=200;
        _recv_status=RECV_HTTP_LINE;
        _request.Reset();
    }
    int GetRespStatus()
    {
        return _resp_status;
    }
    HttpRecvStatus GetRecvStatus()
    {
        return _recv_status;
    }
    HttpRequest& GetRequest()
    {
        return _request;
    }
    void RecvHttpRequest(Buffer* buff)
    {
        switch(_recv_status)
        {
            //不同状态处理不同的事，但不能break，因为处理完头部应该立刻跳转处理正文
            case RECV_HTTP_LINE:
            if(!RecvHttpLine(buff))
            return;
            if(_recv_status!=RECV_HTTP_HEAD)
            return;
            case RECV_HTTP_HEAD:
            if(!RecvHttpHead(buff))
            return;
            if(_recv_status!=RECV_HTTP_BODY)
            return;
            case RECV_HTTP_BODY:
            if(!RecvHttpBody(buff))
            return;
        }
        return;
    }
};