#ifndef OPTIONAL_H
#define OPTIONAL_H

#include <utility>
#include <stdexcept>
#include <type_traits>

template <typename T>
class Optional {
public:
    Optional() : has_value(false) {}

    Optional(const T& value) : has_value(true), value(value) {}

    Optional(T&& value) : has_value(true), value(std::move(value)) {}

    bool has_value() const { return has_value; }

    T& value() {
        if (!has_value) {
            throw std::runtime_error("Attempt to access value in an empty Optional");
        }
        return op_value;
    }

    const T& value() const {
        if (!has_value) {
            throw std::runtime_error("Attempt to access value in an empty Optional");
        }
        return op_value;
    }

    // 支持从 nullptr 或 nullopt_t 转换为 Optional
    Optional& operator=(std::nullptr_t) {
        has_value = false;
        return *this;
    }

private:
    bool has_value;
    T op_value;
};

// nullopt_t 表示没有值的状态
struct nullopt_t { explicit nullopt_t() = default; };

// 这里改为 static const nullopt，避免使用 C++17 的内联变量
static const nullopt_t nullopt{};

// 定义 nullopt 常量
template <typename T>
Optional<T> make_optional() {
    return Optional<T>{};
}

template <typename T>
Optional<T> make_optional(T&& value) {
    return Optional<T>{std::forward<T>(value)};
}

#endif // OPTIONAL_H
