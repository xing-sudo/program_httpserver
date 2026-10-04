#pragma once
#include<string>
#include<unordered_map>



class HttpResponse // 储存HTTP响应信息要素:状态码，响应头部，响应内容,重定向
{
public:
    int _status;                                           // 状态码
    std::string _body;                                     // 响应正文
    bool _redirect_flag;                                   // 是否重定向
    std::string _redirect_url;                             // 重定向地址
    std::unordered_map<std::string, std::string> _headers; // 响应头部
public:
    HttpResponse():_redirect_flag(false),_status(200){}
    HttpResponse(int status):_redirect_flag(false),_status(status){}
    void Reset()
    {
        _status = 200;
        _redirect_flag = false;
        _body.clear();
        _redirect_url.clear();
        _headers.clear();
    }
    // 插入头部字段
    void SetHeader(const std::string &key, const std::string &value)
    {
        _headers.insert(std::make_pair(key, value));
    }
    // 判断是否存在对应的字段
    bool Hasheader(const std::string &key) const
    {
        auto it = _headers.find(key);
        if (it != _headers.end())
        {
            return true;
        }
        return false;
    }
    // 获取指定字段的值
    std::string GetHeader(const std::string &key) const
    {
        auto it = _headers.find(key);
        if (it != _headers.end())
        {
            return it->second;
        }
        return "";
    }
    // 设置正文
    void Setcontent(const std::string &body, const std::string &type = "text/html")
    {
        _body = body;
        SetHeader("Content-Type", type);
    }
    // 设置重定向
    void SetRedirect(const std::string &url, int status = 302)
    {
        _status = status;
        _redirect_flag = true;
        _redirect_url = url;
    }
    bool Close()
    {
        if (Hasheader("Connection") == true && GetHeader("Connection") == "keep-alive")
        {
            return false;
        }
        return true;
    }
};