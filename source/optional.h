#ifndef OPTIONAL_H
#define OPTIONAL_H

#include <utility>
#include <stdexcept>
#include <type_traits>

// nullopt_t 用于表示空值
struct nullopt_t { explicit nullopt_t() = default; };
static const nullopt_t nullopt{};  // 定义 nullopt 常量

template <typename T>
class Optional {
public:
    // 默认构造函数，表示没有值
    Optional() : op_has_value(false) {}

    // 用值构造 Optional
    Optional(const T& value) : op_has_value(true), op_value(value) {}
    Optional(T&& value) : op_has_value(true), op_value(std::move(value)) {}

    // 接受 nullopt_t 类型的构造函数，表示没有值
    Optional(nullopt_t) : op_has_value(false) {}

    // 判断是否有值
    bool has_value() const { return op_has_value; }

    // 获取值，抛出异常如果没有值
    T& value() {
        if (!op_has_value) {
            throw std::runtime_error("Attempt to access value in an empty Optional");
        }
        return op_value;
    }

    const T& value() const {
        if (!op_has_value) {
            throw std::runtime_error("Attempt to access value in an empty Optional");
        }
        return op_value;
    }

    // 支持从 nullptr 或 nullopt_t 转换为 Optional
    Optional& operator=(std::nullptr_t) {
        op_has_value = false;
        return *this;
    }

    // 重载操作符!，使得 !Optional 可以检查是否为空
    explicit operator bool() const {
        return op_has_value;  // 如果有值，返回 true；否则返回 false
    }

    bool operator!() const {
        return !op_has_value;  // 如果没有值，返回 true，否则返回 false
    }

    // 重载逻辑运算符 || 和 &&，使得可以与 Optional 一起使用
    bool operator||(const Optional<T>& other) const {
        return has_value() || other.has_value();
    }

    bool operator&&(const Optional<T>& other) const {
        return has_value() && other.has_value();
    }

    // 额外添加的 value_or 方法，类似于 std::optional 的 value_or 方法
    T value_or(const T& default_value) const {
        return op_has_value ? op_value : default_value;
    }

private:
    bool op_has_value;  // 标记是否有值
    T op_value;         // 存储值
};

// 静态工厂方法，返回一个空的 Optional
template <typename T>
Optional<T> make_optional() {
    return Optional<T>{};
}

template <typename T>
Optional<T> make_optional(T&& value) {
    return Optional<T>{std::forward<T>(value)};
}

// 返回一个空的 Optional<T> 对象
template <typename T>
Optional<T> empty_optional() {
    return Optional<T>{nullopt};
}

#endif // OPTIONAL_H
