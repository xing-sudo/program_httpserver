#pragma once
#include<functional>
#include<regex>
#include"tcpServer.h"





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