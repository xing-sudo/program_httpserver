#ifndef XING_LOGGER_H
#define XING_LOGGER_H

#include <ctime>
#include <cstdio>
#include <pthread.h>

// 日志级别常量
#define INF 0
#define DBG 1
#define ERR 2
// 阈值允许被构建系统覆盖：压测时用 -DLOG_LEVEL=99 彻底关掉日志
// （原来四个宏共用一个 #ifndef，导致外部一旦定义 LOG_LEVEL，INF/DBG/ERR 就缺失而编译失败）
#ifndef LOG_LEVEL
#define LOG_LEVEL INF
#endif

// 宏函数定义：LOG(level, format, ...)
#define LOG(level, format, ...)                                                                                       \
    do                                                                                                                \
    {                                                                                                                 \
        if (level < LOG_LEVEL)                                                                                        \
            break;                                                                                                    \
        time_t t = time(nullptr);                                                                                     \
        struct tm *ltm = localtime(&t);                                                                               \
        char tmp[32] = {0};                                                                                           \
        strftime(tmp, 31, "%H:%M:%S", ltm);                                                                           \
        fprintf(stdout, "[%p %s %s:%d]" format "\n", (void *)pthread_self(), tmp, __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)

#define INF_LOG(format, ...) LOG(INF, format, ##__VA_ARGS__)
#define DBG_LOG(format, ...) LOG(DBG, format, ##__VA_ARGS__)
#define ERR_LOG(format, ...) LOG(ERR, format, ##__VA_ARGS__)

#endif // XING_LOGGER_H
