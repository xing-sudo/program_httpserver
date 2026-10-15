#include "http_request.h"
#include "logger.h"
void HttpRequest::Reset()
{
    _method.clear();
    _path.clear();
    _version = "HTTP/1.1";
    _body.clear();
    std::smatch match;
    _matches.swap(match);
    _headers.clear();
    _params.clear();
}

size_t HttpRequest::ContentLength() const
{
    // 获取Content-Length字段
    bool ret = Hasheader("Content-Length");
    if (ret == false)
    {
        return 0;
    }
    std::string len = GetHeader("Content-Length");
    long res=0;
    try{
        res=std::stol(len);
    }catch(const std::invalid_argument&)
    {
        ERR_LOG("content_length is invalid");
        return 0;
    }catch(const std::out_of_range&)
    {
        ERR_LOG("content_length is invalid");
        return 0;
    }
    if(res<0)
    return 0;

    return res;
}

bool HttpRequest::Close() const
{
    // 没有connection字段或者值为close都是短链接
    if (Hasheader("Connection") == true && GetHeader("Connection") == "keep-alive")
    {
        return false;
    }
    return true;
}
