#include "http_request.h"
#include "logger.h"

#include <charconv>
#include <system_error>

namespace
{
    std::string_view TrimOws(std::string_view value)
    {
        // 去掉选项的空格和制表符
        const auto first = value.find_first_not_of(" \t"); // 从头开始找
        if (first == std::string_view::npos)
            return {};
        const auto last = value.find_last_not_of(" \t"); // 从尾部开始找
        return value.substr(first, last - first + 1);
    }

    bool EqualsIgnoreCase(std::string_view lhs, std::string_view rhs)
    {
        if (lhs.size() != rhs.size())
            return false;
        for (size_t i = 0; i < lhs.size(); ++i)
        {
            // HTTP 选项使用 ASCII；不改变保存的原始字段值（全程值传递）。
            const auto lower = [](char c)
            {
                return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
            };
            if (lower(lhs[i]) != lower(rhs[i]))
                return false;
        }
        return true;
    }

    bool ParseUnsigned(std::string_view text, unsigned &value)
    {
        if (text.empty())
            return false;
        const auto result = std::from_chars(text.data(), text.data() + text.size(), value); // from_cahr(start,end,int)降[start,end)区域的字符转换成整型放入int中，本身不分配内存，不抛异常
        return result.ec == std::errc{} && result.ptr == text.data() + text.size();
        // 严格要求，转化必须成功，且必须全部解析完成
    }
} // namespace

bool HttpRequest::HttpVersion::Parse(std::string_view text)
{
    if (!text.starts_with("HTTP/"))
        return false;
    text.remove_prefix(5); // remove_prefix只是移动视图的起点不修改原字符串
    const auto dot = text.find('.');
    if (dot == std::string_view::npos)
        return false;

    unsigned parsed_major = 0;
    unsigned parsed_minor = 0;
    if (!ParseUnsigned(text.substr(0, dot), parsed_major) ||
        !ParseUnsigned(text.substr(dot + 1), parsed_minor))
        return false;

    major = parsed_major;
    minor = parsed_minor;
    return true;
}

std::string HttpRequest::HttpVersion::ToString() const
{
    return "HTTP/" + std::to_string(major) + "." + std::to_string(minor);
}

bool HttpRequest::SetVersion(std::string_view text)
{
    return _version.Parse(text);
}

std::string HttpRequest::GetVersion() const
{
    return _version.ToString();
}

void HttpRequest::Reset()
{
    _method.clear();
    _path.clear();
    _version = HttpVersion{};
    _body.clear();
    std::smatch match;
    _matches.swap(match);
    _headers.clear();
    _params.clear();
    hasContentLength=false;
    hasTransferEncoding=false;
}
bool HttpRequest::SetHeader(std::string key, std::string_view value)
{
    // key值统一转成小写存储，再去掉key中不合法的字段，查找哈希表有没有存这个key，重复key特殊处理，不重复再插入
    if (key == "")
    {
        DBG_LOG("HttpRequest::SetHeader: the key is null skip");
        return true;
    }
    for (auto &e : key)
    {
        e = static_cast<char>(std::tolower(static_cast<unsigned char>(e)));
    }
    value = TrimOws(value);
    auto it = _headers.find(key);
    if (it != _headers.end())
    {
        // Connection有多个选项，close，upgrade，keep-alive，尤其是 close。
        if (key == "connection")
        {
            it->second.push_back(',');
            it->second.append(value);
            return true;//success
        }
        return false;//insert fail
    }
    if(key=="transfer-encoding")
    {
        hasTransferEncoding=true;
    }
    if(key=="content-length")
    {
        hasContentLength=true;
    }
    _headers.emplace(std::move(key), std::string(value));
    return true;
}

int HttpRequest::ContentLength(size_t* res) const
{
    // 获取Content-Length字段
    bool ret = Hasheader("Content-Length");
    if (ret == false)
    {
        return 0;
    }
    std::string_view len = GetHeader("Content-Length");
    if(len.data()==nullptr)
    {
        ERR_LOG("content_length is invalid:the value is null");
        return 1;//bad request
    }
   
    const auto& frcr=std::from_chars(len.data(),len.data()+len.size(),*res);
    if(frcr.ec==std::errc(0)&&frcr.ptr==len.data()+len.size())
    {
        return 0;
    }else if(frcr.ec==std::errc::invalid_argument || frcr.ptr!=len.data()+len.size())
    {
        ERR_LOG("HttpRequest::ContentLength: ContentLength is invalid");
        return 1;
    }else if(frcr.ec==std::errc::result_out_of_range)
    {
        ERR_LOG("HttpRequest::ContentLength: ContentLength is invalid: the value is outflow ");
        return -1;
    }
    return 1;
}

bool HttpRequest::HasConnectionToken(std::string_view wanted) const
{
    const auto it = _headers.find("connection");
    if (it == _headers.end() || wanted.empty())
        return false;

    std::string_view remaining = it->second;
    while (!remaining.empty())
    {
        const auto comma = remaining.find(',');
        const auto token = TrimOws(remaining.substr(0, comma));
        if (EqualsIgnoreCase(token, wanted))
            return true;
        if (comma == std::string_view::npos)
            break;
        remaining.remove_prefix(comma + 1);
    }
    return false;
}

bool HttpRequest::Close() const
{
    //先假设不需要关闭，只处理要关闭的情况
    bool isClose = false;
    // 此处只负责 HTTP/1.x；其他主版本需要对应的协议处理器。
    if (_version.major != 1 || HasConnectionToken("close"))//1.0进来没有带close这个不执行，1.1进来不带close也不执行
        isClose = true;

    // HTTP/1.0 显式保活；HTTP/1.1 及兼容的后续次版本默认保活。
    if (_version.minor == 0 && !HasConnectionToken("keep-alive"))//特意拦截1.0看看有没有keep-alive没有就关闭
        isClose = true;

    return isClose;
}
