#include "buffer.h"
#include "any.h"

#include <iostream>
#include <string>

int main()
{
    Buffer buf;
    buf.Write("Hello, World!", 13);
    uint64_t len = buf.ReadAbleSize();
    std::cout<<"len:"<<len<<std::endl;

    Any tmp(buf);
    Buffer* tmp_buf=tmp.Get<Buffer>();
    std::string str(len, '\0');
    if (!tmp_buf->Read(&str[0], len)) {
        return 1;
    }
    std::cout << "str:" << str << std::endl;
}
