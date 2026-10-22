#pragma once

#include "buffer.h"
#include "http_request.h"

#include <string>

typedef enum
{
    RECV_HTTP_ERROR=0,
    RECV_HTTP_LINE,
    RECV_HTTP_HEAD,
    RECV_HTTP_BODY,
    RECV_HTTP_OVER
} HttpRecvStatus;

// 请求更新上下文，接收的数据不是一条完整的数据
class Httpcontext
{
private:
    int _resp_status;            // 响应状态码
    HttpRecvStatus _recv_status; // 接收及解析状态
    HttpRequest _request;  // 已经解析得到的请求信息
    size_t nowBytes=0;//对于这条请求已经处理请求头的字节数
    size_t nowCount=0;//对于这条请求已经处理的请求头个数
private:
    inline void Fail(int status)
    {
        _resp_status=status;
        _recv_status=RECV_HTTP_ERROR;
    }
    bool RecvHttpLine(Buffer *buff);
    bool RecvHttpHead(Buffer *buff);
    bool RecvHttpBody(Buffer *buff);
    bool ParseHttpLine(const std::string &line);
    bool ParseHttpHead(std::string &head);
public:
    Httpcontext() : _resp_status(200), _recv_status(RECV_HTTP_LINE) {}
    void Reset()
    {
        _resp_status = 200;
        _recv_status = RECV_HTTP_LINE;
        _request.Reset();
        nowBytes=0;
        nowCount=0;
    }
    int GetRespStatus()
    {
        return _resp_status;
    }
    HttpRecvStatus GetRecvStatus()
    {
        return _recv_status;
    }
    HttpRequest &GetRequest()
    {
        return _request;
    }
    void RecvHttpRequest(Buffer *buff);
};
