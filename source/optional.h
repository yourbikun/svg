#pragma once

#include <iostream>
#include <stdexcept>
#include <type_traits>

template <typename T>
class Optional {
public:
    // 默认构造函数，表示没有值
    Optional() : has_value_(false) {}

    // 构造函数，接收一个值
    Optional(const T& value) : has_value_(true), value_(value) {}

    Optional(T&& value) : has_value_(true), value_(std::move(value)) {}

    // 拷贝构造函数
    Optional(const Optional& other) : has_value_(other.has_value_), value_(other.value_) {}

    // 移动构造函数
    Optional(Optional&& other) noexcept : has_value_(other.has_value_), value_(std::move(other.value_)) {
        other.has_value_ = false;
    }

    // 赋值操作符
    Optional& operator=(const Optional& other) {
        if (this != &other) {
            has_value_ = other.has_value_;
            value_ = other.value_;
        }
        return *this;
    }

    // 移动赋值操作符
    Optional& operator=(Optional&& other) noexcept {
        if (this != &other) {
            has_value_ = other.has_value_;
            value_ = std::move(other.value_);
            other.has_value_ = false;
        }
        return *this;
    }

    // 获取值，如果没有值，抛出异常
    T& operator*() {
        if (!has_value_) {
            throw std::runtime_error("Attempted to access value of an empty Optional.");
        }
        return value_;
    }

    const T& operator*() const {
        if (!has_value_) {
            throw std::runtime_error("Attempted to access value of an empty Optional.");
        }
        return value_;
    }

    // 获取值的指针，如果没有值，返回 nullptr
    T* operator->() {
        return has_value_ ? &value_ : nullptr;
    }

    const T* operator->() const {
        return has_value_ ? &value_ : nullptr;
    }

    // 判断是否有值
    explicit operator bool() const {
        return has_value_;
    }

    // 获取值，如果没有值，抛出异常
    T& value() {
        if (!has_value_) {
            throw std::runtime_error("Attempted to access value of an empty Optional.");
        }
        return value_;
    }

    // const version of value()
    const T& value() const {
        if (!has_value_) {
            throw std::runtime_error("Attempted to access value of an empty Optional.");
        }
        return value_;
    }

    // 清空 Optional
    void reset() {
        has_value_ = false;
        value_ = T();
    }

    // 获取是否有值
    bool has_value() const {
        return has_value_;
    }

private:
    bool has_value_;
    T value_;
};

// 全局变量，表示没有值的 Optional
struct nullopt_t { explicit nullopt_t() = default; };
inline constexpr nullopt_t nullopt{};

// 使 Optional 可以与 nullopt 配合使用
template <typename T>
Optional<T> make_optional(nullopt_t) {
    return Optional<T>();
}

template <typename T>
Optional<T> operator=(Optional<T>& opt, nullopt_t) {
    opt.reset();
    return opt;
}

