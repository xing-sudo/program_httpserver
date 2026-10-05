#pragma once

#include <cstdint>
#include <string>
#include <vector>

class Buffer
{
private:
    uint64_t _write_pos;
    uint64_t _read_pos;
    std::vector<char> _data;

public:
    Buffer();
    Buffer &operator=(const Buffer &other);
    char *Head();
    char *GetWritePos();
    char *GetReadPos();
    uint64_t TailSpace();
    uint64_t HeadSpace();
    uint64_t ReadAbleSize();
    void MoveReadPos(uint64_t len);
    void MoveWritePos(uint64_t len);
    void EnsureSpace(uint64_t len);
    void Write(const void *data, uint64_t len);
    void Write(Buffer &buf);
    void Write(const std::string &str);
    bool Read(void *buff, uint64_t len);
    std::string ReadAsstring(uint64_t len);
    char *FindCRLF();
    std::string Getline();
    void clear();
};
