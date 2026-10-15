#pragma once

#include "buffer.h"
#include "http_request.h"
#include "http_response.h"
#include "tcpServer.h"

#include <functional>
#include <regex>
#include <string>
#include <utility>
#include <vector>

// 服务器类，负责监听端口，接收请求，处理请求，返回响应
class HttpServer
{
    // 设计四个请求路由映射表，高性能TCP服务器，静态资源相对根目录
    // 静态资源：取文件内容写回response中，动态：找到具体方法执行函数将结果返回response
private:
    using Handler = std::function<void(const HttpRequest &, HttpResponse *)>;
    using Handlers = std::vector<std::pair<std::regex, Handler>>;
    Handlers _get_route;    // GET请求路由表
    Handlers _post_route;   // POST请求路由表
    Handlers _put_route;    // PUT请求路由表
    Handlers _delete_route; // DELETE请求路由表
    std::string _basedir;   // 静态资源根目录
    TcpServer _server;
private:
    void ErrorHandler(const HttpRequest &req, HttpResponse *rsp);
    void FileHandler(const HttpRequest &req, HttpResponse *rsp);              // 静态资源处理
    void Dispatcher(HttpRequest &req, HttpResponse *rsp, Handlers &handlers); // 动态资源处理
    bool IsFileHandler(const HttpRequest &req);
    void Route(HttpRequest &req, HttpResponse *rsp);
    void OnConnected(const PtrConnection &conn);            // 连接建立，设置上下文
    void OnMessage(const PtrConnection &conn, Buffer *buf); // buffer内的数据处理
    void WriteResponse(const PtrConnection &conn, const HttpRequest &req, HttpResponse &rsp); // 构造响应
public:
    HttpServer(int port, int timerout);
    void SetBaseDir(const std::string &basedir);
    // 设置四个请求方法的正则表达式和处理函数
    void Get(const std::string &pattern, const Handler &handler)
    {
        _get_route.push_back(std::make_pair(std::regex(pattern), handler));
    }
    void Post(const std::string &pattern, const Handler &handler)
    {
        _post_route.push_back(std::make_pair(std::regex(pattern), handler));
    }
    void Put(const std::string &pattern, const Handler &handler)
    {
        _put_route.push_back(std::make_pair(std::regex(pattern), handler));
    }
    void Delete(const std::string &pattern, const Handler &handler)
    {
        _delete_route.push_back(std::make_pair(std::regex(pattern), handler));
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
