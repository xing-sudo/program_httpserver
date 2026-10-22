#include "http_context.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
void Check(bool condition, const std::string &message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void TestVersions()
{
    HttpRequest request;
    const HttpRequest &read_only = request;
    Check(read_only.GetVersion() == "HTTP/1.1", "default/const version getter");
    for (const std::string version : {"HTTP/1.0", "HTTP/1.1", "HTTP/1.2", "HTTP/2.0"})
    {
        Check(request.SetVersion(version), "parse " + version);
        Check(read_only.GetVersion() == version, "format " + version);
    }
    // 能表示其他版本，不代表 HTTP/1.x 连接策略可以处理其他主版本。
    Check(request.Close(), "HTTP/2 must not use HTTP/1.x keep-alive policy");
    Check(request.SetVersion("HTTP/1.0"), "restore HTTP/1.0");
    for (const std::string invalid : {
             "", "1.1", "HTTP/1", "HTTP/.1", "HTTP/1.", "HTTP/-1.1",
             "HTTP/+1.1", "HTTP/1.-1", "HTTP/1.+1", "HTTP/1.1junk",
             "HTTP/1.1.0", "HTTP/1.1\r\n", "HTTP/1. 1",
             "HTTP/999999999999999999999999999999.1",
             "HTTP/2.999999999999999999999999999999"})
    {
        Check(!request.SetVersion(invalid), "reject invalid version " + invalid);
        Check(read_only.GetVersion() == "HTTP/1.0", "failed parse must preserve version");
    }
    request.Reset();
    Check(read_only.GetVersion() == "HTTP/1.1", "reset version");
}

void TestConnectionOptions()
{
    struct Case
    {
        const char *version;
        const char *connection;
        bool close;
    };
    const Case cases[] = {
        {"HTTP/1.1", nullptr, false},
        {"HTTP/1.0", nullptr, true},
        {"HTTP/1.1", "", false},
        {"HTTP/1.0", " \t ", true},
        {"HTTP/1.1", "keep-alive", false},
        {"HTTP/1.0", "Keep-Alive", false},
        {"HTTP/1.1", "CLOSE", true},
        {"HTTP/1.0", "close", true},
        {"HTTP/1.1", "upgrade", false},
        {"HTTP/1.0", "upgrade", true},
        {"HTTP/1.1", "keep-alive, upgrade", false},
        {"HTTP/1.0", "upgrade, KEEP-ALIVE", false},
        {"HTTP/1.1", "keep-alive, upgrade, CLOSE", true},
        {"HTTP/1.0", "CLOSE, keep-alive, upgrade", true},
        {"HTTP/1.1", "upgrade,\tClose\t, keep-alive", true},
        {"HTTP/1.1", ",, keep-alive, ,", false},
        {"HTTP/1.1", "x-close", false},
        {"HTTP/1.0", "x-keep-alive", true},
        {"HTTP/1.2", nullptr, false},
    };
    for (const auto &test : cases)
    {
        HttpRequest request;
        Check(request.SetVersion(test.version), "connection test version");
        if (test.connection)
            request.SetHeader("CoNnEcTiOn", test.connection);
        Check(request.Close() == test.close,
              std::string(test.version) + " Connection=" + (test.connection ? test.connection : "<absent>"));
    }

    HttpRequest request;
    request.SetHeader("Connection", "keep-alive");
    request.SetHeader("CONNECTION", " \tCLOSE\t ");
    Check(request.HasConnectionToken("KeEp-AlIvE"), "case-insensitive token query");
    Check(request.HasConnectionToken("close"), "duplicate Connection must retain close");
    Check(!request.HasConnectionToken(""), "empty token is not an option");
    Check(request.Close(), "duplicate Connection: close wins");
    request.SetHeader("Authorization", " \tBearer AbC123 ");
    Check(request.GetHeader("AUTHORIZATION") == "Bearer AbC123", "preserve value case/internal space");
}

void TestIncrementalParsing()
{
    Httpcontext context;
    Buffer buffer;
    buffer.Write("GET /hello HTTP/1.");
    context.RecvHttpRequest(&buffer);
    Check(context.GetRecvStatus() == RECV_HTTP_LINE, "wait for fragmented request line");
    buffer.Write("0\r\nConnection: Keep-Alive,\tupgrade\r\nconnection: CLOSE\r\n\r\n");
    context.RecvHttpRequest(&buffer);
    Check(context.GetRecvStatus() == RECV_HTTP_OVER, "complete fragmented request");
    Check(context.GetRequest().GetVersion() == "HTTP/1.0", "wire version assignment");
    Check(context.GetRequest().Close(), "wire duplicate Connection retains close");

    context.Reset();
    buffer.Write("GET /hello HTTP/1.1\r\n\r\n");
    context.RecvHttpRequest(&buffer);
    Check(context.GetRecvStatus() == RECV_HTTP_OVER, "parse after context reset");
    Check(!context.GetRequest().Close(), "HTTP/1.1 defaults to persistence");

    context.Reset();
    buffer.Write("GET /hello HTTP/2.0\r\n\r\n");
    context.RecvHttpRequest(&buffer);
    Check(context.GetRespStatus() == 400, "HTTP/1 parser must not silently claim HTTP/2 support");
}
} // namespace

int main()
{
    try
    {
        TestVersions();
        TestConnectionOptions();
        TestIncrementalParsing();
        std::cout << "HTTP version, Connection and incremental parsing checks passed\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
