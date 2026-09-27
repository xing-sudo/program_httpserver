// HTTP Server 测试 main
// 编译命令：g++ -o http_server http_main.cpp -std=c++11 -lpthread -O2
// 运行前需要创建静态资源目录：mkdir -p ./wwwroot && echo "<h1>Hello HTTP Server!</h1>" > ./wwwroot/index.html
#include "http.cpp"

int main()
{
    // 1. 创建 HttpServer，端口 8080，60秒无活动超时
    HttpServer server(8080, 60);

    // 2. 设置静态资源根目录（提前创建好 ./wwwroot 目录）
    server.SetBaseDir("./wwwroot");

    // 3. 设置线程数（建议设置为 CPU 核心数）
    server.SetThreadCount(4);

    // ---------------------------------------------------------------
    // 4. 注册动态路由
    // ---------------------------------------------------------------

    // GET /hello  ——  返回纯文本
    server.Get("/hello", [](const HttpRequest& req, HttpResponse* rsp) {
        rsp->Setcontent("<h1>Hello, World!</h1>", "text/html");
        rsp->_status = 200;
    });

    // GET /info  ——  返回请求信息（调试用）
    server.Get("/info", [](const HttpRequest& req, HttpResponse* rsp) {
        std::string body;
        body += "<html><body>";
        body += "<h2>Request Info</h2>";
        body += "<p>Method: " + req._method + "</p>";
        body += "<p>Path: " + req._path + "</p>";
        body += "<p>Version: " + req._version + "</p>";
        body += "<h3>Headers:</h3><ul>";
        for (auto& h : req._headers) {
            body += "<li>" + h.first + ": " + h.second + "</li>";
        }
        body += "</ul>";
        if (!req._params.empty()) {
            body += "<h3>Query Params:</h3><ul>";
            for (auto& p : req._params) {
                body += "<li>" + p.first + " = " + p.second + "</li>";
            }
            body += "</ul>";
        }
        body += "</body></html>";
        rsp->Setcontent(body, "text/html");
        rsp->_status = 200;
    });

    // GET /echo?msg=xxx  ——  把查询参数 msg 回显回去
    server.Get("/echo", [](const HttpRequest& req, HttpResponse* rsp) {
        std::string msg = req.GetParam("msg");
        if (msg.empty()) {
            rsp->Setcontent("<p>请在 URL 后面加 ?msg=你的消息</p>", "text/html");
        } else {
            rsp->Setcontent("<p>你说的是：" + msg + "</p>", "text/html");
        }
        rsp->_status = 200;
    });

    // POST /submit  ——  接收 POST body 并回显
    server.Post("/submit", [](const HttpRequest& req, HttpResponse* rsp) {
        std::string body;
        body += "<html><body>";
        body += "<h2>POST Body Received:</h2>";
        body += "<pre>" + req._body + "</pre>";
        body += "</body></html>";
        rsp->Setcontent(body, "text/html");
        rsp->_status = 200;
    });

    // GET /redirect  ——  301 重定向到 /hello
    server.Get("/redirect", [](const HttpRequest& req, HttpResponse* rsp) {
        rsp->SetRedirect("/hello", 301);
    });

    // ---------------------------------------------------------------
    // 5. 启动服务器（阻塞）
    // ---------------------------------------------------------------
    printf("[HttpServer] Listening on port 8080...\n");
    printf("[HttpServer] Static dir: ./wwwroot\n");
    printf("[HttpServer] Routes: /hello  /info  /echo?msg=xxx  /submit(POST)  /redirect\n");
    server.Listen();

    return 0;
}
