项目介绍：基于 C++11、Linux epoll 的 Reactor HTTP 服务器
已实现功能：主从 Reactor、eventfd 跨线程通知、输入输出缓冲区、HTTP 解析与路由、时间轮
目录结构：base、net、http、app 
base，net，http内部都包含include，src子目录
base负责提供基础的模块支持主要有日志，缓冲区，以及手写的类型擦除器用于存放任意类型上下文
构建与运行：

当前状态与计划：正在完善协议边界、生命周期和测试，后续推进流式推理网关
