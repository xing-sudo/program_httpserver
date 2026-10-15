#pragma once

#include <cstddef>
#include <regex>
#include <string>
#include <unordered_map>
#include <utility>

// 储存HTTP信息要素:请求方法，请求路径，请求版本，请求头部，请求内容，content-length,长短连接
class HttpRequest
{
public:
    std::string _method;                                   // 请求方法
    std::string _path;                                     // 请求路径
    std::string _version;                                  // 版本
    std::string _body;                                     // 正文
    std::smatch _matches;                                  // 资源路径的正则提取数据
    std::unordered_map<std::string, std::string> _headers; // 头部
    std::unordered_map<std::string, std::string> _params;  // 查询字符串
public:
    HttpRequest() : _version("HTTP/1.1") {}
    void Reset();
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
    size_t ContentLength() const; // 获取正文长度
    bool Close() const;           // 判断是否是短链接
};
