#pragma once
#include <utility>
#include <iostream>
#include <stdexcept>

template <typename T>
class Optional {
public:
    // 默认构造函数，表示没有值
    Optional() : has_value_(false) {}

    // 有值的构造函数
    Optional(const T& value) : has_value_(true), value_(value) {}

    Optional(T&& value) : has_value_(true), value_(std::move(value)) {}

    // 拷贝构造函数
    Optional(const Optional& other) : has_value_(other.has_value_), value_(other.value_) {}

    // 移动构造函数
    Optional(Optional&& other) noexcept : has_value_(other.has_value_), value_(std::move(other.value_)) {
        other.has_value_ = false;
    }

    // 拷贝赋值
    Optional& operator=(const Optional& other) {
        if (this != &other) {
            has_value_ = other.has_value_;
            value_ = other.value_;
        }
        return *this;
    }

    // 移动赋值
    Optional& operator=(Optional&& other) noexcept {
        if (this != &other) {
            has_value_ = other.has_value_;
            value_ = std::move(other.value_);
            other.has_value_ = false;
        }
        return *this;
    }

    // 检查是否有值
    bool has_value() const { return has_value_; }

    // 获取值，若没有值则抛出异常
    T& value() {
        if (!has_value_) {
            throw std::runtime_error("Accessing value of an empty Optional.");
        }
        return value_;
    }

    const T& value() const {
        if (!has_value_) {
            throw std::runtime_error("Accessing value of an empty Optional.");
        }
        return value_;
    }

    // 获取值，若没有值则返回默认值
    T value_or(const T& default_value) const {
        return has_value_ ? value_ : default_value;
    }

    // 重载 bool 运算符，便于条件判断
    explicit operator bool() const { return has_value_; }

    // 重载解引用运算符（可选）
    T& operator*() {
        return value();
    }

    const T& operator*() const {
        return value();
    }

    // 重载箭头运算符（可选）
    T* operator->() {
        return &value();
    }

    const T* operator->() const {
        return &value();
    }

private:
    bool has_value_;
    T value_;
};
