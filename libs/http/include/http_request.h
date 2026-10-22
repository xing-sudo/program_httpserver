#pragma once

#include <cstddef>
#include <cctype>
#include <regex>
#include <string>
#include <unordered_map>
#include <utility>
#include <string_view>
// 储存HTTP信息要素:请求方法，请求路径，请求版本，请求头部，请求内容，content-length,长短连接

class HttpRequest
{
public:
    struct HttpVersion
    {
        unsigned major = 1;
        unsigned minor = 1;

        // 只解析版本表示；是否支持该协议由协议处理层决定。
        // 失败时返回 false，保留原来的版本，不抛数值转换异常。
        bool Parse(std::string_view text);
        std::string ToString() const;
    };
    bool hasContentLength = false;
    bool hasTransferEncoding = false;
    std::string _method;                                   // 请求方法
    std::string _path;                                     // 请求路径
    HttpVersion _version;                                  // 版本
    std::string _body;                                     // 正文
    std::smatch _matches;                                  // 资源路径的正则提取数据
    std::unordered_map<std::string, std::string> _headers; // 头部
    std::unordered_map<std::string, std::string> _params;  // 查询字符串
public:
    HttpRequest() = default;
    void Reset();
    bool SetVersion(std::string_view text);
    std::string GetVersion() const;
    // 按逗号逐项解析 Connection，裁剪空白并忽略选项大小写。
    bool HasConnectionToken(std::string_view token) const;
    bool SetHeader(std::string key, std::string_view value);
    inline int getExistCLAndTE()
    {
        int res=0;
        if(hasTransferEncoding)
        {
            res=1;
        }
        if(hasTransferEncoding&&hasContentLength)
        {
            res=2;
        }
        return res;
    }

    // 判断是否存在对应的字段
    bool Hasheader(const std::string &key) const
    {
        std::string tmpkey = key;
        for (auto &e : tmpkey)
        {
            e = static_cast<char>(std::tolower(static_cast<unsigned char>(e)));
        }
        auto it = _headers.find(tmpkey);
        if (it != _headers.end())
        {
            return true;
        }
        return false;
    }
    // 获取字段对应的值
    std::string_view GetHeader(std::string_view key) const
    {
        // auto iter=isSame(key);
        // if(iter==_headers.end())
        // {
        //     return "";
        // }
        // return (*iter).second;
        std::string tmpkey(key);
        for (auto &e : tmpkey)
        {
            e = static_cast<char>(std::tolower(static_cast<unsigned char>(e)));
        }
        auto it = _headers.find(tmpkey);
        if (it != _headers.end())
        {
            return it->second;
        }
        return {};
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
    int ContentLength(size_t *res) const; // 获取正文长度
    bool Close() const;                   // 判断是否是短链接
};
