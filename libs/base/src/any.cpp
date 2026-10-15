#include"any.h"

Any::Any() : _content(nullptr)
{
}
Any::Any(const Any& other):_content(other._content?other._content->clone():nullptr)
{
}
Any::~Any()
{
    if(_content)
    {
        delete _content;
        _content = nullptr;
    }
}
Any& Any::swap( Any& other)
{
    std::swap(_content,other._content);
    return *this;
}
Any& Any::operator=(const Any& other)
{
    Any(other).swap(*this);
    return *this;
}
