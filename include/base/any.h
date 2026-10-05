#pragma once

#include <cassert>
#include <typeinfo>
#include <utility>

class Any
{
private:
    class holder
    {
    public:
        virtual ~holder() {}
        virtual holder *clone() const = 0;
        virtual const std::type_info &type() const = 0;
    };

    template <class T>
    class placeholder : public holder
    {
    public:
        T _data;
        explicit placeholder(const T &data) : _data(data) {}
        holder *clone() const override { return new placeholder(_data); }
        const std::type_info &type() const override { return typeid(T); }
    };

    holder *_content;

public:
    Any();
    template <class T>
    Any(const T &val) : _content(new placeholder<T>(val)) {}
    Any(const Any &other);
    ~Any();
    Any &swap(Any &other);
    Any &operator=(const Any &other);

    template <class T>
    T *Get()
    {
        assert(_content && typeid(T) == _content->type());
        return &static_cast<placeholder<T> *>(_content)->_data;
    }

    template <class T>
    Any &operator=(const T &val)
    {
        Any(val).swap(*this);
        return *this;
    }
};
