#pragma once
#include<string>
#include<regex>
#include<unordered_map>




class HttpRequest // 储存HTTP信息要素:请求方法，请求路径，请求版本，请求头部，请求内容，content-length,长短连接
{
public:
    std::string _method;                                   // 请求方法
    std::string _path;                                     // 请求路径
    std::string _version;                                   // 版本
    std::string _body;                                     // 正文
    std::smatch _matches;                                  // 资源路径的正则提取数据
    std::unordered_map<std::string, std::string> _headers; // 头部
    std::unordered_map<std::string, std::string> _params;  // 查询字符串
public:
    HttpRequest() : _version("HTTP/1.1") {}
    void Reset()
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
    // 获取字段对应的值
    std::string GetHeader(const std::string &key) const
    {
        auto it = _headers.find(key);
        if (it != _headers.end())
        {
            return it->second;
        }
        return "";
    }
    // 插入查询字符串
    void SetParam(const std::string &key, const std::string &value)
    {
        _params.insert(std::make_pair(key, value));
    }
    // 判断是否存在对应的查询字符串
    bool HasParam(const std::string &key) const
    {
        auto it = _params.find(key);
        if (it != _params.end())
        {
            return true;
        }
        return false;
    }
    // 获取查询字符串对应的值
    std::string GetParam(const std::string &key) const
    {
        auto it = _params.find(key);
        if (it != _params.end())
        {
            return it->second;
        }
        return "";
    }
    // 获取正文长度
    size_t ContentLength() const
    {
        // 获取Content-Length字段
        bool ret = Hasheader("Content-Length");
        if (ret == false)
        {
            return 0;
        }
        std::string len = GetHeader("Content-Length");
        return std::stol(len);
    }
    // 判断是否是短链接
    bool Close() const
    {
        // 没有connection字段或者值为close都是短链接
        if (Hasheader("Connection") == true && GetHeader("Connection") == "keep-alive")
        {
            return false;
        }
        return true;
    }
};