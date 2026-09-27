// 此模块应该包含Util工具类，HttpRequest类：保存解析后的请求信息，HttpResponse类：保存响应信息，HttpContext类：保存上下文信息防止请求不完整。
// Util应该包括文件读写，URL编码和解码，状态码及其描述内容的映射，后缀名获取文件类型，判断是否是目录，判断是否是普通文件，资源路径有效性的判断。
#include <unordered_map>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <regex>
#include"source.cpp"

const std::unordered_map<int, std::string> _statu_msg = {
    {100, "Continue"},
    {101, "Switching Protocol"},
    {102, "Processing"},
    {103, "Early Hints"},
    {200, "OK"},
    {201, "Created"},
    {202, "Accepted"},
    {203, "Non-Authoritative Information"},
    {204, "No Content"},
    {205, "Reset Content"},
    {206, "Partial Content"},
    {207, "Multi-Status"},
    {208, "Already Reported"},
    {226, "IM Used"},
    {300, "Multiple Choice"},
    {301, "Moved Permanently"},
    {302, "Found"},
    {303, "See Other"},
    {304, "Not Modified"},
    {305, "Use Proxy"},
    {306, "unused"},
    {307, "Temporary Redirect"},
    {308, "Permanent Redirect"},
    {400, "Bad Request"},
    {401, "Unauthorized"},
    {402, "Payment Required"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {406, "Not Acceptable"},
    {407, "Proxy Authentication Required"},
    {408, "Request Timeout"},
    {409, "Conflict"},
    {410, "Gone"},
    {411, "Length Required"},
    {412, "Precondition Failed"},
    {413, "Payload Too Large"},
    {414, "URI Too Long"},
    {415, "Unsupported Media Type"},
    {416, "Range Not Satisfiable"},
    {417, "Expectation Failed"},
    {418, "I'm a teapot"},
    {421, "Misdirected Request"},
    {422, "Unprocessable Entity"},
    {423, "Locked"},
    {424, "Failed Dependency"},
    {425, "Too Early"},
    {426, "Upgrade Required"},
    {428, "Precondition Required"},
    {429, "Too Many Requests"},
    {431, "Request Header Fields Too Large"},
    {451, "Unavailable For Legal Reasons"},
    {501, "Not Implemented"},
    {502, "Bad Gateway"},
    {503, "Service Unavailable"},
    {504, "Gateway Timeout"},
    {505, "HTTP Version Not Supported"},
    {506, "Variant Also Negotiates"},
    {507, "Insufficient Storage"},
    {508, "Loop Detected"},
    {510, "Not Extended"},
    {511, "Network Authentication Required"}};

const std::unordered_map<std::string, std::string> _mime_msg = {
    {".aac", "audio/aac"},
    {".abw", "application/x-abiword"},
    {".arc", "application/x-freearc"},
    {".avi", "video/x-msvideo"},
    {".azw", "application/vnd.amazon.ebook"},
    {".bin", "application/octet-stream"},
    {".bmp", "image/bmp"},
    {".bz", "application/x-bzip"},
    {".bz2", "application/x-bzip2"},
    {".csh", "application/x-csh"},
    {".css", "text/css"},
    {".csv", "text/csv"},
    {".doc", "application/msword"},
    {".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {".eot", "application/vnd.ms-fontobject"},
    {".epub", "application/epub+zip"},
    {".gif", "image/gif"},
    {".htm", "text/html"},
    {".html", "text/html"},
    {".ico", "image/vnd.microsoft.icon"},
    {".ics", "text/calendar"},
    {".jar", "application/java-archive"},
    {".jpeg", "image/jpeg"},
    {".jpg", "image/jpeg"},
    {".js", "text/javascript"},
    {".json", "application/json"},
    {".jsonld", "application/ld+json"},
    {".mid", "audio/midi"},
    {".midi", "audio/x-midi"},
    {".mjs", "text/javascript"},
    {".mp3", "audio/mpeg"},
    {".mpeg", "video/mpeg"},
    {".mpkg", "application/vnd.apple.installer+xml"},
    {".odp", "application/vnd.oasis.opendocument.presentation"},
    {".ods", "application/vnd.oasis.opendocument.spreadsheet"},
    {".odt", "application/vnd.oasis.opendocument.text"},
    {".oga", "audio/ogg"},
    {".ogv", "video/ogg"},
    {".ogx", "application/ogg"},
    {".otf", "font/otf"},
    {".png", "image/png"},
    {".pdf", "application/pdf"},
    {".ppt", "application/vnd.ms-powerpoint"},
    {".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
    {".rar", "application/x-rar-compressed"},
    {".rtf", "application/rtf"},
    {".sh", "application/x-sh"},
    {".svg", "image/svg+xml"},
    {".swf", "application/x-shockwave-flash"},
    {".tar", "application/x-tar"},
    {".tif", "image/tiff"},
    {".tiff", "image/tiff"},
    {".ttf", "font/ttf"},
    {".txt", "text/plain"},
    {".vsd", "application/vnd.visio"},
    {".wav", "audio/wav"},
    {".weba", "audio/webm"},
    {".webm", "video/webm"},
    {".webp", "image/webp"},
    {".woff", "font/woff"},
    {".woff2", "font/woff2"},
    {".xhtml", "application/xhtml+xml"},
    {".xls", "application/vnd.ms-excel"},
    {".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {".xml", "application/xml"},
    {".xul", "application/vnd.mozilla.xul+xml"},
    {".zip", "application/zip"},
    {".3gp", "video/3gpp"},
    {".3g2", "video/3gpp2"},
    {".7z", "application/x-7z-compressed"}};

class Util
{
public:
    static bool ReadFile(const std::string &filename, std::string *buff)
    {
        // 读取文件内容到buff
        std::ifstream ifs(filename, std::ios::binary);
        if (ifs.is_open() == false)
        {
            printf("open %s failed\n", filename.c_str());
            return false;
        }
        size_t fsize = 0;
        ifs.seekg(0, ifs.end); // 跳转到文件末尾
        fsize = ifs.tellg();   // 获取文件起始位置相对于当前位置的偏移量
        ifs.seekg(0, ifs.beg); // 跳转到开头
        buff->resize(fsize);
        ifs.read(&(*buff)[0], fsize);
        if (ifs.good() == false)
        {
            printf("read %s failed\n", filename.c_str());
            ifs.close();
            return false;
        }
        ifs.close();
        return true;
    }
   static bool WriteFile(const std::string &filename, const std::string &buff)
    {
        // 将buff写入文件
        std::ofstream ofs(filename, std::ios::binary | std::ios::trunc);
        if (ofs.is_open() == false)
        {
            printf("open %s failed\n", filename.c_str());
            return false;
        }
        ofs.write(buff.c_str(), buff.size());
        if (ofs.good() == false)
        {
            printf("write %s failed\n", filename.c_str());
            ofs.close();
            return false;
        }
        ofs.close();
        return true;
    }
    // URL编码，避免URL中资源路径与查询字符串中的特殊字符与HTTP请求中特殊字符产生歧义
    // 编码格式：将特殊字符的ascii值，转换为两个16进制字符，前缀%   C++ -> C%2B%2B
    //   不编码的特殊字符： RFC3986文档规定 . - _ ~ 字母，数字属于绝对不编码字符
    // RFC3986文档规定，编码格式 %HH
    // W3C标准中规定，查询字符串中的空格，需要编码为+， 解码则是+转空格
    static std::string UrlEncode(const std::string &url, bool convert)
    {
        std::string res;
        for (auto &c : url)
        {
            // 当字符为._-~ 普通数字和字母时追加 isalnum（）用于判断字符是否是字母或者数字
            if (c == '.' || c == '_' || c == '~' || c == '-' || isalnum(c))
            {
                res += c;
                continue;
            }
            if (c == ' ' && convert == true)
            {
                res += '+';
                continue;
            }
            // 其余字符要编码成%HH形式，两位十六进制数
            char tmp[4] = {0};
            snprintf(tmp, 4, "%%%02X", (unsigned char)c); //%02X表示以2位十六进制数表示
            res += tmp;
        }
        return res;
    }
    static char HEXTOI(char c)
    {
        // 将16进制字符转换为数字
        if (c >= '0' && c <= '9')
        {
            return c - '0';
        }
        else if (c >= 'a' && c <= 'z')
        {
            return c - 'a' + 10; //+10是因为a~f对应10~15
        }
        else if (c >= 'A' && c <= 'Z')
        {
            return c - 'A' + 10;
        }
        return -1;
    }
    static std::string UrlDecode(const std::string &url, bool convert)
    {
        // 遇到%，则将紧随其后的两个字符转换为数字第一个左移4位+第二个
        std::string res;
        for (int i = 0; i < url.size(); i++)
        {
            if (url[i] == '+' && convert == true)
            {
                res += ' ';
                continue;
            }
            if (url[i] == '%' && (i + 2) < url.size())
            {
                char v1 = HEXTOI(url[i + 1]);
                char v2 = HEXTOI(url[i + 2]);
                char v = v1 * 16 + v2;
                res += v;
                i += 2;
                continue;
            }
            res += url[i];
        }
        return res;
    }

    static std::string StatusDesc(int status)
    {
        auto it = _statu_msg.find(status);
        if (it != _statu_msg.end())
        {
            return it->second;
        }
        return "Unknow";
    }
    static std::string ExtMime(const std::string &filename)
    {
        // 获取后缀再分割子串
        size_t pos = filename.find_last_of('.');
        if (pos == std::string::npos)
        {
            return "application/octet-stream";
        }
        std::string ext = filename.substr(pos);
        auto it = _mime_msg.find(ext);
        if (it == _mime_msg.end())
        {
            return "application/octet-stream";
        }
        return it->second;
    }
    static bool IsDir(const std::string &filename)
    {
        struct stat st;
        int ret = stat(filename.c_str(), &st);
        if (ret < 0)
        {
            return false;
        }
        return S_ISDIR(st.st_mode);
    }
   static bool Isregular(const std::string &filename)
    {
        struct stat st;
        int ret = stat(filename.c_str(), &st);
        if (ret < 0)
        {
            return false;
        }
        return S_ISREG(st.st_mode);
    }
   static bool ValidPath(const std::string &path)
    {
        //../算违规，利用深度进行合理性判断
        std::vector<std::string> subdir;
        Split(path, "/", &subdir);
        int level = 0;
        for (auto &s : subdir)
        {
            if (s == "..")
            {
                level--;
                if (level < 0)
                    return false;
                continue;
            }
            level++;
        }
        return true;
    }
    static size_t Split(const std::string &src, const std::string &sep, std::vector<std::string> *arr)
    {
        // 改函数按照分隔符sep将src分割成数组arr，并返回数组大小
        size_t offset = 0;
        while (offset < src.size())
        {
            size_t pos = src.find(sep, offset);
            if (pos == std::string::npos)
            {
                arr->push_back(src.substr(offset));
                return arr->size();
            }
            if (pos == offset)
            {
                offset = pos + sep.size();
                continue; // 跳过空字符串
            }
            arr->push_back(src.substr(offset, pos - offset));
            offset = pos + sep.size();
        }
        return arr->size();
    }
};

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

typedef enum
{
    RECV_HTTP_ERROR,
    RECV_HTTP_LINE,
    RECV_HTTP_HEAD,
    RECV_HTTP_BODY,
    RECV_HTTP_OVER
} HttpRecvStatus;

#define MAX_LINE 8192 // 最大行长度
class Httpcontext // 请求更新上下文，接收的数据不是一条完整的数据
{
private:
    int _resp_status;            // 响应状态码
    HttpRecvStatus _recv_status; // 接收及解析状态
    HttpRequest _request;        // 已经解析得到的请求信息
private:
    bool RecvHttpLine(Buffer *buff)
    {
        if(_recv_status!=RECV_HTTP_LINE)
        {
            return false;
        }
        std::string line=buff->Getline();
        if(line.size()==0)
        {   //如果buff中的可读数据比最大长度还大但还是不足一行，那一定是有问题的
            if(buff->ReadAbleSize()>MAX_LINE)
            {
                _recv_status=RECV_HTTP_ERROR;
                _resp_status=414;//URI Too Long
                return false;
            }
            //数据不够再等等
            return true;
        }
        if(line.size()>MAX_LINE)
        {
            _recv_status=RECV_HTTP_ERROR;
            _resp_status=414;//URI Too Long
            return false;
        }
        bool ret=ParseHttpLine(line);
        if(ret==false)
        {
            return false;
        }
        //首行处理完毕，进入头部处理
        _recv_status=RECV_HTTP_HEAD;
        return true;
     }
    bool RecvHttpHead(Buffer *buff)
    {
        if(_recv_status!=RECV_HTTP_HEAD)
        {
            return false;
        }
        while(1)
        {
            std::string line=buff->Getline();
            if(line.size()==0)
            {
                if(buff->ReadAbleSize()>MAX_LINE)
                {
                    _recv_status=RECV_HTTP_ERROR;
                    _resp_status=414;//URI Too Long
                    return false;
                }
                return true;
                
            }
            if(line.size()>MAX_LINE)//数据太长处理不了
            {
                _recv_status=RECV_HTTP_ERROR;
                _resp_status=414;//URI Too Long
                return false;
            }
            if(line=="\n"||line=="\r\n")//读到空行，头部结束
            {
                break;
            }
             bool ret=ParseHttpHead(line);
             if(ret==false)
             {
                return false;
             }
        }
        _recv_status=RECV_HTTP_BODY;
        return true;
    }
    bool RecvHttpBody(Buffer *buff)
    {
        if(_recv_status!=RECV_HTTP_BODY)
        {
            return false;
        }
        //获取正文长度
        size_t len=_request.ContentLength();
        if(len==0)
        {
            _recv_status=RECV_HTTP_OVER;
            return true;
        }
        //获取实际需要读取的长度
        int real_len=len-_request._body.size();
        //缓冲区内的数据很多，就读完
        if(buff->ReadAbleSize()>= real_len)
        {
            _request._body.append(buff->GetReadPos(),real_len);
            buff->MoveReadPos(real_len);
            _recv_status=RECV_HTTP_OVER;
            return true;
        }
        //缓冲区的数据不满足需要读取的长度，就有多少读多少，等待下次
        _request._body.append(buff->GetReadPos(),buff->ReadAbleSize());
        buff->MoveReadPos(buff->ReadAbleSize());
        return true;
    }   
    bool ParseHttpLine(const std::string& line)
    {   //正则匹配
        std::smatch matches;
        std::regex e("(GET|POST|PUT|HEAD|DELETE) ([^?]*)(?:\\?(.*))? (HTTP/1\\.[01])(?:\n|\r\n)?",std::regex::icase);
        bool ret= std::regex_match(line,matches,e);
        if(ret==false)
        {
            _recv_status=RECV_HTTP_ERROR;
            _resp_status=400;//Bad Request
            return false;
        }
        _request._method=matches[1];//请求方法
        _request._path=Util::UrlDecode(matches[2],false);//请求路径
        _request._version=matches[4];//版本
        //提取查询字符串
        std::vector<std::string> query_string_arr;
        std::string query_str=matches[3];
        //查询字符串的格式是key1=value1&key2=value2按&将其分割
        Util::Split(query_str,"&",&query_string_arr);
        //针对各个子串 以 = 分割得到key和value 进行解码后插入
        for(auto& str:query_string_arr)
        {
            size_t pos=str.find("=");
            if(pos==std::string::npos)
            {
                _recv_status=RECV_HTTP_ERROR;
                _resp_status=400;//Bad Request
                return false;
            }
            std::string key=Util::UrlDecode(str.substr(0,pos),true);
            std::string value=Util::UrlDecode(str.substr(pos+1),true);
            _request.SetParam(key,value);
        }
        return true;
    }
    bool ParseHttpHead( std::string& head)
    {
        //末尾是\r或\n去掉
        if(head.back()=='\r')
        {  
            head.pop_back();
        }
        if(head.back()=='\n')
        {
            head.pop_back();
        }
        size_t pos=head.find(": ");
        if(pos==std::string::npos)
        {
            _recv_status=RECV_HTTP_ERROR;
            _resp_status=400;//Bad Request
            return false;
        }
        std::string key=head.substr(0,pos);
        std::string value=head.substr(pos+2);
        _request.SetHeader(key,value);
        return true;
    }
public:
    Httpcontext():_resp_status(200),_recv_status(RECV_HTTP_LINE){}
    void Reset()
    {
        _resp_status=200;
        _recv_status=RECV_HTTP_LINE;
        _request.Reset();
    }
    int GetRespStatus()
    {
        return _resp_status;
    }
    HttpRecvStatus GetRecvStatus()
    {
        return _recv_status;
    }
    HttpRequest& GetRequest()
    {
        return _request;
    }
    void RecvHttpRequest(Buffer* buff)
    {
        switch(_recv_status)
        {
            //不同状态处理不同的事，但不能break，因为处理完头部应该立刻跳转处理正文
            case RECV_HTTP_LINE:
            if(!RecvHttpLine(buff))
            return;
            if(_recv_status!=RECV_HTTP_HEAD)
            return;
            case RECV_HTTP_HEAD:
            if(!RecvHttpHead(buff))
            return;
            if(_recv_status!=RECV_HTTP_BODY)
            return;
            case RECV_HTTP_BODY:
            if(!RecvHttpBody(buff))
            return;
        }
        return;
    }
};
class HttpServer // 服务器类，负责监听端口，接收请求，处理请求，返回响应
{
    //设计四个请求路由映射表，高性能TCP服务器，静态资源相对根目录
    //静态资源：夺取文件内容写回response中，动态：找到具体方法执行函数将结果返回response
    private:
    using Handler =std::function<void(const HttpRequest&,HttpResponse*)>;
    using Handlers=std::vector<std::pair<std::regex,Handler>>;
    Handlers _get_route;//GET请求路由表
    Handlers _post_route;//POST请求路由表
    Handlers _put_route;///PUT请求路由表
    Handlers _delete_route;//DELETE请求路由表
    std::string _basedir;//静态资源根目录
    TcpServer _server;
    private:
    void ErrorHandler(const HttpRequest& req,HttpResponse* rsp )
    {
        //组织错误页面
        std::string body;
            body += "<html>";
            body += "<head>";
            body += "<meta http-equiv='Content-Type' content='text/html;charset=utf-8'>";
            body += "</head>";
            body += "<body>";
            body += "<h1>";
            body += std::to_string(rsp->_status);
            body += " ";
            body += Util::StatusDesc(rsp->_status);
            body += "</h1>";
            body += "</body>";
            body += "</html>";
            //设置响应
            rsp->Setcontent(body);
    }
    void FileHandler(const HttpRequest& req,HttpResponse* rsp)//静态资源处理
    {   //将文件中的数据读取出来并放入response中，设置mime
        std::string req_path=_basedir+req._path;
        if(req._path.back()=='/')
        {
            req_path+="index.html";
        }
        bool ret=Util::ReadFile(req_path,&rsp->_body);
        if(ret==false)
        {
            return;
        }
        std::string mime=Util::ExtMime(req_path);
        rsp->SetHeader("Content-Type",mime);
        return;
    }
    void Dispatcher(HttpRequest& req,HttpResponse* rsp,Handlers& handlers)//动态资源处理
    {
        //在对应的路由表中查找匹配的正则表达式，执行对应的处理函数
        //如果没有匹配的，则返回404错误
        for(auto & handler:handlers)
        {
            const std::regex& re=handler.first;
            const Handler& functor=handler.second;
            bool ret=std::regex_match(req._path,req._matches,re);
            if(ret==false)
            {
                continue;
            }
            return functor(req,rsp);
        }
        rsp->_status=404;//Not Found
    }
    bool IsFileHandler(const HttpRequest& req)
    {   //必须有根目录
        if(_basedir.empty())
        {
            return false;
        }
        //请求方法必须是GET或HEAD
        if(req._method!="GET"&&req._method!="HEAD")
        {
            return false;
        }
        //请求路径必须是合法路径
        if(Util::ValidPath(req._path)==false)
        {
            return false;
        }
        
        std::string req_path=_basedir+req._path;
        if(req._path.back()=='/')
        {
            req_path+="index.html";
        }
        //请求路径必须是文件
        if(Util::Isregular(req_path)==false)
        {
            return false;
        }
        return true;
    }
    void Route( HttpRequest& req,HttpResponse* rsp)
    {
        //对请求进行分辨
        if(IsFileHandler(req)==true)
        {
            return FileHandler(req,rsp);
        }
        if(req._method=="GET"||req._method=="HEAD")
        {
            return Dispatcher(req,rsp,_get_route);
        }else if(req._method=="POST")
        {
            return Dispatcher(req,rsp,_post_route);
        }else if(req._method=="PUT")
        {
            return Dispatcher(req,rsp,_put_route);
        }else if(req._method=="DELETE")
        {
            return Dispatcher(req,rsp,_delete_route);
        }
        rsp->_status=405;//Method Not Allowed
        return;
    }
    void OnConnected(const PtrConnection&conn)//连接建立，设置上下文
    {
        conn->SetContext(Httpcontext());
        DBG_LOG("NEW CONNECTION %p",conn.get());
    }
    void OnMessage(const PtrConnection&conn,Buffer* buf)//buffer内的数据处理
    {
        while(buf->ReadAbleSize()>0)
        {
        //1.获取上下文
        Httpcontext* context=conn->GetContext()->Get<Httpcontext>();
        //2.通过上下文对buffer数据进行解析,得到request对象
        //(1).缓冲区数据解析错误，直接回复错误响应
        context->RecvHttpRequest(buf);
        HttpRequest& req=context->GetRequest();
        HttpResponse rsp(context->GetRespStatus());
        if(context->GetRespStatus()>=400)
        {   //错误响应
            ErrorHandler(req,&rsp);
            WriteResponse(conn,req,rsp);
            context->Reset();
            buf->MoveReadPos(buf->ReadAbleSize());
            conn->Shutdown();
            return;
        }
        //(2).解析成功，根据request对象进行路由处理
        if(context->GetRecvStatus()!=RECV_HTTP_OVER )
        {   //数据未接收完毕
            return;
        }
        //3.请求路由+逻辑处理
        Route(req,&rsp);
        //4.HttpResponse发送
        WriteResponse(conn,req,rsp);
        //5.上下文重置
        context->Reset();
        //6.长短连接判断2
        if(rsp.Close()==true)
        {
            conn->Shutdown();
        }
    }
        return;
    }
    void WriteResponse(const PtrConnection&conn,const HttpRequest& req, HttpResponse& rsp)//构造响应
    {
        //1.构造响应头部
        if(req.Close()==true)
        {
            rsp.SetHeader("Connection","Close");
        }else{
            rsp.SetHeader("Connection","Keep-Alive");
        }
        if(rsp._body.empty()==false&&rsp.Hasheader("Content-Length")==false)
        {
            rsp.SetHeader("Content-Length",std::to_string(rsp._body.size()));
        }
        if(rsp._body.empty()==false&&rsp.Hasheader("Content-Type")==false)
        {
            rsp.SetHeader("Content-Type","application/octet-stream");
        }
        if(rsp._redirect_flag==true)
        {
            rsp.SetHeader("Location",rsp._redirect_url);
        }
        //2.将rsp中的要素按照http的协议格式进行组织
        std::stringstream rsp_str;
        rsp_str<<req._version<<" "<<std::to_string(rsp._status)<<" "<<Util::StatusDesc(rsp._status)<<"\r\n";
        for(auto& head:rsp._headers)
        {
            rsp_str<<head.first<<": "<<head.second<<"\r\n";
        }
        rsp_str<<"\r\n";
        rsp_str<<rsp._body;
        //3.发送响应
        conn->Send(rsp_str.str().c_str(),rsp_str.str().size());
        return;
    }
    public:
    HttpServer(int port,int timerout):
    _server(port)
    {
        _server.EnableInactiveRelease(timerout);
        _server.SetConnectedCallBack(std::bind(&HttpServer::OnConnected,this,std::placeholders::_1));
        _server.SetMessageCallBack(std::bind(&HttpServer::OnMessage,this,std::placeholders::_1,std::placeholders::_2));
    }
    void SetBaseDir(const std::string& basedir)
    {
        assert(Util::IsDir(basedir)==true);
        _basedir=basedir;
    }
    //设置四个请求方法的正则表达式和处理函数
    void Get(const std::string &pattern,const Handler& handler)
    {
        _get_route.push_back(std::make_pair(std::regex(pattern),handler));
    }
    void Post(const std::string &pattern,const Handler& handler)
    {
        _post_route.push_back(std::make_pair(std::regex(pattern),handler));
    }
     void Put(const std::string &pattern,const Handler& handler)
    {
        _put_route.push_back(std::make_pair(std::regex(pattern),handler));
    }
    void Delete(const std::string &pattern,const Handler& handler)
    {
        _delete_route.push_back(std::make_pair(std::regex(pattern),handler));
    }
    void SetThreadCount(int count)
    {
        _server.SetThreadCount(count);
    }
    void Listen()
    {
        _server.Start();
    }
};