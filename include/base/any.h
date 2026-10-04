#include <typeinfo>

// 处理任何类型的请求（协议）上下文
// 不能使用模板因为模板在实例化时必须确定类型
class Any
{
    // 采用父子继承的方式实现Any中保存父的指针
    // 通过父类的虚函数实现对不同类型数据的操作
private:
    class holder
    {
    public:
        virtual ~holder() {}                            // 父类的析构函数必须是虚函数，否则子类对象被销毁时不会调用子类的析构函数，导致资源泄漏
        virtual holder *clone() const = 0;              // 克隆函数用于复制对象
        virtual const std::type_info &type() const = 0; // 获取对象类型信息
    };
    template <class T>
    class placeholder : public holder
    {
    public:
        T _data;

    public:
        placeholder(const T &data) : _data(data) {}
        virtual holder *clone() const override
        {
            return new placeholder(_data);
        }
        virtual const std::type_info &type() const override
        {
            return typeid(T);
        }
    };

private:
    holder *_content; // 保存任意类型数据的指针
public:
    Any() : _content(nullptr) {}
    template <class T> // 模板构造
    Any(const T &val) : _content(new placeholder<T>(val))
    {
    }
    // 拷贝构造函数实现深拷贝
    Any(const Any &other) : _content(other._content ? other._content->clone() : nullptr) {}
    ~Any()
    {
        if (_content)
        {
            delete _content;
            _content = nullptr;
        }
    }
    Any &swap(Any &other)
    {
        std::swap(_content, other._content);
        return *this;
    }
    template <class T>
    T *Get()
    {
        assert(typeid(T) == _content->type());
        return &((placeholder<T> *)_content)->_data;
    }
    template <class T>
    Any &operator=(const T &val)
    {
        Any(val).swap(*this);
        return *this;
    }
    Any &operator=(const Any &other)
    {
        Any(other).swap(*this);
        return *this;
    }
};