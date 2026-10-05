#include "buffer.h"

#include <cassert>
#include <cstring>

    Buffer::Buffer() : _write_pos(0), _read_pos(0), _data(1024) {}
    Buffer &Buffer::operator=(const Buffer &other) // 赋值重载
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
    char *Buffer::Head() // 获取容器头位置
    {
        return _data.data();
    }
    char *Buffer::GetWritePos()
    {
        return Head() + _write_pos; // 写位置指针
    }
    char *Buffer::GetReadPos()
    {
        return Head() + _read_pos; // 读位置指针
    }
    uint64_t Buffer::TailSpace()
    {
        return _data.size() - _write_pos; // 尾部剩余空间
    }
    uint64_t Buffer::HeadSpace()
    {
        return _read_pos; // 头部剩余空间
    }
    uint64_t Buffer::ReadAbleSize()
    {
        return _write_pos - _read_pos; // 可读数据大小
    }
    // 移动读位置
    void Buffer::MoveReadPos(uint64_t len)
    {
        assert(len <= ReadAbleSize());
        _read_pos += len;
    }
    // 移动写位置
    void Buffer::MoveWritePos(uint64_t len)
    {
        assert(len <= TailSpace());
        _write_pos += len;
    }
    // 确保有足够的空间写入数据
    void Buffer::EnsureSpace(uint64_t len)
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
            std::memmove(Head(), GetReadPos(), readable_size); // 将可读数据移动到头部
            _read_pos = 0;                                                 // 更新读位置
            _write_pos = readable_size;                                    // 更新写位置
        }
        else
        {
            _data.resize(_write_pos + len); // 否则，扩展容器大小
        }
    }
    void Buffer::Write(const void *data, uint64_t len)
    {
        if (len == 0)
        {
            return;
        }

        EnsureSpace(len);                                                       // 确保有足够空间写入数据
        std::memcpy(GetWritePos(), data, len); // 将数据写入写位置
        MoveWritePos(len);                                                      // 更新写位置
    }
    void Buffer::Write( Buffer & buf)
    {
        uint64_t len = buf.ReadAbleSize();
        if(len == 0)
        {
            return;
        }                     
        if(this==&buf)
        {
            std::vector<char> temp(buf.GetReadPos(),buf.GetReadPos()+len);
            Write(temp.data(),temp.size());
            return;
        }                             // 确保有足够空间写入数据
        return Write(buf.GetReadPos(),len);
    }
    // 写入字符串
    void Buffer::Write(const std::string &str)
    {
        Write(str.c_str(), str.size());
    }
    // 读取数据
    bool Buffer::Read(void *buff, uint64_t len)
    {
        if(len>ReadAbleSize()||(len!=0&&buff==nullptr))
        {
            return false;
        }
        if(len!=0)
        {
            std::memcpy(buff, GetReadPos(), len);
        }
        MoveReadPos(len);
        return true;
    }
    // 读取指定长度的数据并返回字符串
    std::string Buffer::ReadAsstring(uint64_t len)
    {
        assert(len <= ReadAbleSize());
        if (len == 0) {
            return {};
        }
        std::string str;
        str.resize(len);
        Read(&str[0], len);
        return str;
    }
    // 查找CRLF位置
    // memchr()
    char *Buffer::FindCRLF()
    {
        char *pos = (char *)memchr(GetReadPos(), '\n', ReadAbleSize()); // 查找换行符
        return pos;
    }
    // 读取一行数据，直到CRLF
    std::string Buffer::Getline()
    {
        char *pos = FindCRLF();
        if (pos == nullptr)
        {
            return "";
        }
        return ReadAsstring(pos - GetReadPos() + 1);
    }
    // 清空缓冲区
    void Buffer::clear()
    {
        _write_pos = 0;
        _read_pos = 0;
    }
