#include "http_context.h"

#include <regex>
#include <string>
#include <vector>

#include "http_utility.h"

constexpr size_t max_line_size = 8192u; // 最大行长度
constexpr std::size_t max_body_size = 30u * 1024u * 1024u;
constexpr size_t max_header_bytes = 64u * 1024u;
constexpr size_t max_header_count = 100u;

bool Httpcontext::RecvHttpLine(Buffer *buff)
{
    if (_recv_status != RECV_HTTP_LINE)
    {
        return false;
    }
    std::string line = buff->Getline();
    if (line.size() == 0)
    { // 如果buff中的可读数据比最大长度还大但还是不足一行，那一定是有问题的
        if (buff->ReadAbleSize() > max_line_size)
        {
            Fail(414); // URI Too Long
            return false;
        }
        // 数据不够再等等
        return true;
    }
    if (line.size() > max_line_size)
    {

        Fail(414); // URI Too Long
        return false;
    }
    bool ret = ParseHttpLine(line);
    if (ret == false)
    {
        return false;
    }
    // 首行处理完毕，进入头部处理
    _recv_status = RECV_HTTP_HEAD;
    return true;
}

bool Httpcontext::RecvHttpHead(Buffer *buff)
{
    if (_recv_status != RECV_HTTP_HEAD)
    {
        return false;
    }
    while (1)
    {
        if (nowBytes > max_header_bytes)
        {
            ERR_LOG("Httpcontext::RecvHttpHead: now handle header byte[%lu] is out of max bytes", nowBytes);
            Fail(431);
            return false;
        }
        std::string line = buff->Getline();
        if (line.empty())
        {
            const size_t remainBytes = buff->ReadAbleSize();
            if (remainBytes > max_header_bytes - nowBytes || remainBytes > max_line_size)
            {
                ERR_LOG("Httpcontext::RecvHttpHead: Whilst processing a line, no CRLF has been received after a considerable delay, and the data in the buffer has exceeded the limit");
                Fail(431); // bad request
                return false;
            }
            return true;
        }
        if (line.size() > max_header_bytes - nowBytes)
        {
            ERR_LOG("Httpcontext::RecvHttpHead: the line exceed max limit expect[%lu],now[%lu]", line.size(), max_header_bytes);
            Fail(431);
            return false;
        }
        nowBytes += line.size();
        if (line.size() > max_line_size) // 数据太长处理不了
        {
            Fail(431); // URI Too Long
            return false;
        }
        if (line == "\n" || line == "\r\n") // 读到空行，头部结束
        {
            break;
        }
        if (nowCount >= max_header_count)
        {
            ERR_LOG("Httpcontext::RecvHttpHead: now handle header count[%lu] is out of max count", nowCount);
            Fail(431);
            return false;
        }
        bool ret = ParseHttpHead(line);
        if (ret == false)
        {
            return false;
        }
        nowCount++;
    }
    int res = _request.getExistCLAndTE();
    if (res == 2)
    {
        ERR_LOG("Httpcontext::RecvHttpHead: the both of TE and CL exist at the same time");
        Fail(400);
        return false;
    }
    else if (res == 1)
    {
        ERR_LOG("Httpcontext::RecvHttpHead: we server is not support TE");
        Fail(501);
        return false;
    }
    _recv_status = RECV_HTTP_BODY;
    return true;
}

bool Httpcontext::RecvHttpBody(Buffer *buff)
{
    if (_recv_status != RECV_HTTP_BODY)
    {
        return false;
    }
    // 获取正文长度
    size_t len = 0;
    int res = _request.ContentLength(&len);
    if (len == 0 && res == 0)
    {
        _recv_status = RECV_HTTP_OVER;
        return true;
    }

    if (res == 1)
    {
        Fail(400);
        return false;
    }

    if (res == -1)
    {
        Fail(413);
        return false;
    }

    if (len > max_body_size)
    {
        ERR_LOG("ContentLength is invalid:the value is exceed to max");
        Fail(413);
        return false;
    }
    // 获取实际需要读取的长度
    size_t real_len = len - _request._body.size();
    // 缓冲区内的数据很多，就读完
    if (buff->ReadAbleSize() >= real_len)
    {
        _request._body.append(buff->GetReadPos(), real_len);
        buff->MoveReadPos(real_len);
        _recv_status = RECV_HTTP_OVER;
        return true;
    }
    // 缓冲区的数据不满足需要读取的长度，就有多少读多少，等待下次
    _request._body.append(buff->GetReadPos(), buff->ReadAbleSize());
    buff->MoveReadPos(buff->ReadAbleSize());
    return true;
}

bool Httpcontext::ParseHttpLine(const std::string &line)
{ // 正则匹配
    std::smatch matches;
    static const std::regex e("(GET|POST|PUT|HEAD|DELETE) ([^?]*)(?:\\?(.*))? (HTTP/1\\.[01])(?:\n|\r\n)?", std::regex::icase);
    bool ret = std::regex_match(line, matches, e);
    if (ret == false)
    {
        ERR_LOG("Httpcontext::ParseHttpLine: match regex fail");
        Fail(400); // Bad Request
        return false;
    }
    _request._method = matches[1];                       // 请求方法
    _request._path = Util::UrlDecode(matches[2], false); // 请求路径
    if (!_request.SetVersion(matches[4].str()))          // 版本格式错误
    {
        ERR_LOG("Httpcontext::ParseHttpLine: parse HTTP version fail");
        Fail(400);
        return false;
    }
    // 提取查询字符串
    std::vector<std::string> query_string_arr;
    std::string query_str = matches[3];
    // 查询字符串的格式是key1=value1&key2=value2按&将其分割
    Util::Split(query_str, "&", &query_string_arr);
    // 针对各个子串 以 = 分割得到key和value 进行解码后插入
    for (auto &str : query_string_arr)
    {
        size_t pos = str.find("=");
        if (pos == std::string::npos)
        {
            ERR_LOG("Httpcontext::ParseHttpLine: set Param fail");
            Fail(400); // Bad Request
            return false;
        }
        std::string key = Util::UrlDecode(str.substr(0, pos), true);
        std::string value = Util::UrlDecode(str.substr(pos + 1), true);
        _request.SetParam(key, value);
    }
    return true;
}

bool Httpcontext::ParseHttpHead(std::string &head)
{
    // 末尾是\r或\n去掉
    if (head.back() == '\n')
    {
        head.pop_back();
    }
    if (head.back() == '\r')
    {
        head.pop_back();
    }
    size_t pos = head.find(":");
    if (pos == std::string::npos)
    {
        ERR_LOG("Httpcontext::ParseHttpHead: request header without split");
        Fail(400); // Bad Request
        return false;
    }
    std::string key = head.substr(0, pos);
    std::string value = head.substr(pos + 1);
    if (!_request.SetHeader(key, value))
    {
        ERR_LOG("Httpcontext::ParseHttpHead: insert a same value");
        Fail(400);
        return false;
    }
    return true;
}

void Httpcontext::RecvHttpRequest(Buffer *buff)
{
    switch (_recv_status)
    {
        // 不同状态处理不同的事，但不能break，因为处理完头部应该立刻跳转处理正文
    case RECV_HTTP_LINE:
        if (!RecvHttpLine(buff))
            return;
        if (_recv_status != RECV_HTTP_HEAD)
            return;
    case RECV_HTTP_HEAD:
        if (!RecvHttpHead(buff))
            return;
        if (_recv_status != RECV_HTTP_BODY)
            return;
    case RECV_HTTP_BODY:
        if (!RecvHttpBody(buff))
            return;
    }
    return;
}
