#pragma once
#include"logger.h"
#include <vector>
#include<string>
#include<assert.h>

#define BUFFER_SIZE 1024


class Buffer
{
private:
    uint64_t _write_pos;     // 写位置
    uint64_t _read_pos;      // 读位置
    std::vector<char> _data; // 容器

public:
    Buffer() : _write_pos(0), _read_pos(0), _data(BUFFER_SIZE) {}
    Buffer &operator=(const Buffer &other) // 赋值重载
    {
        if (this != &other)
        {
            _write_pos = other._write_pos;
            _read_pos = other._read_pos;
            _data = other._data; // vector的赋值操作会自动处理内存
        }
        return *this;
        // 实际上这里的赋值操作应该是深拷贝
        //_data=new vector<char>(*other._data); // 通过复制构造函数创建一个新的vector对象
    }
    char *Head() // 获取容器头位置
    {
        return &*_data.begin(); // data.begin()返回一个指向vector第一个元素的迭代器，&*操作符将其转换为指针
    }
    char *GetWritePos()
    {
        return Head() + _write_pos; // 写位置指针
    }
    char *GetReadPos()
    {
        return Head() + _read_pos; // 读位置指针
    }
    uint64_t TailSpace()
    {
        return _data.size() - _write_pos; // 尾部剩余空间
    }
    uint64_t HeadSpace()
    {
        return _read_pos; // 头部剩余空间
    }
    uint64_t ReadAbleSize()
    {
        return _write_pos - _read_pos; // 可读数据大小
    }
    // 移动读位置
    void MoveReadPos(uint64_t len)
    {
        assert(len <= ReadAbleSize());
        _read_pos += len;
    }
    // 移动写位置
    void MoveWritePos(uint64_t len)
    {
        assert(len <= TailSpace());
        _write_pos += len;
    }
    // 确保有足够的空间写入数据
    void EnsureSpace(uint64_t len)
    {
        if (TailSpace() >= len) // 如果尾部空间足够，直接返回
        {
            return;
        }
        // 如果头部和尾部空间之和足够，将可读数据移动到头部
        // copy()
        if (HeadSpace() + TailSpace() >= len)
        {
            uint64_t readable_size = ReadAbleSize();                       // 保留可读数据大小
            std::copy(GetReadPos(), GetReadPos() + readable_size, Head()); // 将可读数据移动到头部
            _read_pos = 0;                                                 // 更新读位置
            _write_pos = readable_size;                                    // 更新写位置
        }
        else
        {
            _data.resize(_write_pos + len); // 否则，扩展容器大小
        }
    }
    void Write(const void *data, uint64_t len)
    {
        if (len == 0)
        {
            return;
        }

        EnsureSpace(len);                                                       // 确保有足够空间写入数据
        std::copy((const char *)data, (const char *)data + len, GetWritePos()); // 将数据写入写位置
        MoveWritePos(len);                                                      // 更新写位置
    }
    void Write(Buffer & buf)
    {
        return Write(buf.GetReadPos(),buf.ReadAbleSize());
    }
    // 写入字符串
    void Write(const std::string &str)
    {
        Write(str.c_str(), str.size());
    }
    // 读取数据
    void Read(void *buff, uint64_t len)
    {
        assert(len <= ReadAbleSize());
        std::copy(GetReadPos(), GetReadPos() + len, (char *)buff);
        MoveReadPos(len);
    }
    // 读取指定长度的数据并返回字符串
    std::string ReadAsstring(uint64_t len)
    {
        assert(len <= ReadAbleSize());
        std::string str;
        str.resize(len);
        Read(&str[0], len);
        return str;
    }
    // 查找CRLF位置
    // memchr()
    char *FindCRLF()
    {
        char *pos = (char *)memchr(GetReadPos(), '\n', ReadAbleSize()); // 查找换行符
        return pos;
    }
    // 读取一行数据，直到CRLF
    std::string Getline()
    {
        char *pos = FindCRLF();
        if (pos == nullptr)
        {
            return "";
        }
        return ReadAsstring(pos - GetReadPos() + 1);
    }
    // 清空缓冲区
    void clear()
    {
        _write_pos = 0;
        _read_pos = 0;
    }
};