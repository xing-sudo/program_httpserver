#pragma once

#include <cstddef>
#include <string>
#include <vector>

// HTTP 工具类：文件读写、URL 编解码、状态码与 MIME 映射、路径合法性判断
class Util
{
public:
    // 读取文件内容到buff
    static bool ReadFile(const std::string &filename, std::string *buff);
    // 将buff写入文件
    static bool WriteFile(const std::string &filename, const std::string &buff);
    // URL编码，避免URL中资源路径与查询字符串中的特殊字符与HTTP请求中特殊字符产生歧义
    // 编码格式：将特殊字符的ascii值，转换为两个16进制字符，前缀%   C++ -> C%2B%2B
    //   不编码的特殊字符： RFC3986文档规定 . - _ ~ 字母，数字属于绝对不编码字符
    // RFC3986文档规定，编码格式 %HH
    // W3C标准中规定，查询字符串中的空格，需要编码为+， 解码则是+转空格
    static std::string UrlEncode(const std::string &url, bool convert);
    // 将16进制字符转换为数字
    static char HEXTOI(char c);
    static std::string UrlDecode(const std::string &url, bool convert);
    // 状态码 -> 描述内容
    static std::string StatusDesc(int status);
    // 后缀名 -> MIME类型
    static std::string ExtMime(const std::string &filename);
    static bool IsDir(const std::string &filename);
    static bool Isregular(const std::string &filename);
    // ../算违规，利用深度进行合理性判断
    static bool ValidPath(const std::string &path);
    // 该函数按照分隔符sep将src分割成数组arr，并返回数组大小
    static size_t Split(const std::string &src, const std::string &sep, std::vector<std::string> *arr);
};
