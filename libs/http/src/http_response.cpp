#include "http_response.h"

void HttpResponse::Reset()
{
    _status = 200;
    _redirect_flag = false;
    _body.clear();
    _redirect_url.clear();
    _headers.clear();
}

void HttpResponse::Setcontent(const std::string &body, const std::string &type)
{
    _body = body;
    SetHeader("Content-Type", type);
}

void HttpResponse::SetRedirect(const std::string &url, int status)
{
    _status = status;
    _redirect_flag = true;
    _redirect_url = url;
}

bool HttpResponse::Close()
{
    if (Hasheader("Connection") == true && GetHeader("Connection") == "keep-alive")
    {
        return false;
    }
    return true;
}
