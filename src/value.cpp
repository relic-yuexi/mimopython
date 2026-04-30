/**
 * @file value.cpp
 * @brief PyValue implementation with type conversions and arithmetic.
 */
#include "value.h"
#include <cmath>
#include <sstream>
#include <iomanip>

namespace mimo {

PyValue::PyValue() : data_(static_cast<int64_t>(0)), type_(Type::NONE), is_none_(true) {}
PyValue::PyValue(int64_t v) : data_(v), type_(Type::INT) {}
PyValue::PyValue(double v) : data_(v), type_(Type::FLOAT) {}
PyValue::PyValue(bool v) : data_(v), type_(Type::BOOL) {}
PyValue::PyValue(std::string v) : data_(std::move(v)), type_(Type::STRING) {}
PyValue::PyValue(std::shared_ptr<PyFunction> fn) : data_(std::move(fn)), type_(Type::FUNCTION) {}

PyValue PyValue::none() {
    PyValue v;
    v.is_none_ = true;
    v.type_ = Type::NONE;
    return v;
}

int64_t PyValue::as_int() const {
    return std::get<int64_t>(data_);
}

double PyValue::as_float() const {
    return std::get<double>(data_);
}

bool PyValue::as_bool() const {
    return std::get<bool>(data_);
}

const std::string& PyValue::as_string() const {
    return std::get<std::string>(data_);
}

std::shared_ptr<PyFunction> PyValue::as_function() const {
    return std::get<std::shared_ptr<PyFunction>>(data_);
}

double PyValue::to_float() const {
    if (type_ == Type::FLOAT) return as_float();
    if (type_ == Type::INT) return static_cast<double>(as_int());
    if (type_ == Type::BOOL) return as_bool() ? 1.0 : 0.0;
    return 0.0;
}

int64_t PyValue::to_int() const {
    if (type_ == Type::INT) return as_int();
    if (type_ == Type::FLOAT) return static_cast<int64_t>(as_float());
    if (type_ == Type::BOOL) return as_bool() ? 1 : 0;
    return 0;
}

std::string PyValue::to_string() const {
    if (is_none_) return "None";
    switch (type_) {
        case Type::INT: return std::to_string(as_int());
        case Type::FLOAT: {
            std::ostringstream oss;
            double v = as_float();
            if (v == static_cast<int64_t>(v)) {
                oss << std::fixed << std::setprecision(1) << v;
            } else {
                oss << v;
            }
            return oss.str();
        }
        case Type::BOOL: return as_bool() ? "True" : "False";
        case Type::STRING: return as_string();
        case Type::FUNCTION: return "<function " + as_function()->name + ">";
        case Type::NONE: return "None";
    }
    return "";
}

bool PyValue::truthy() const {
    if (is_none_) return false;
    switch (type_) {
        case Type::INT: return as_int() != 0;
        case Type::FLOAT: return as_float() != 0.0;
        case Type::BOOL: return as_bool();
        case Type::STRING: return !as_string().empty();
        case Type::FUNCTION: return true;
        case Type::NONE: return false;
    }
    return false;
}

// Arithmetic
PyValue PyValue::operator+(const PyValue& rhs) const {
    if (type_ == Type::STRING || rhs.type_ == Type::STRING) {
        return PyValue(to_string() + rhs.to_string());
    }
    if ((is_numeric() || type_ == Type::BOOL) && (rhs.is_numeric() || rhs.type_ == Type::BOOL)) {
        if (type_ == Type::FLOAT || rhs.type_ == Type::FLOAT)
            return PyValue(to_float() + rhs.to_float());
        return PyValue(to_int() + rhs.to_int());
    }
    throw std::runtime_error("TypeError: unsupported operand types for +");
}

PyValue PyValue::operator-(const PyValue& rhs) const {
    if (is_numeric() && rhs.is_numeric()) {
        if (type_ == Type::FLOAT || rhs.type_ == Type::FLOAT)
            return PyValue(to_float() - rhs.to_float());
        return PyValue(as_int() - rhs.as_int());
    }
    throw std::runtime_error("TypeError: unsupported operand types for -");
}

PyValue PyValue::operator*(const PyValue& rhs) const {
    if (type_ == Type::STRING && rhs.type_ == Type::INT) {
        std::string result;
        int64_t n = rhs.as_int();
        for (int64_t i = 0; i < n; ++i) result += as_string();
        return PyValue(std::move(result));
    }
    if (is_numeric() && rhs.is_numeric()) {
        if (type_ == Type::FLOAT || rhs.type_ == Type::FLOAT)
            return PyValue(to_float() * rhs.to_float());
        return PyValue(as_int() * rhs.as_int());
    }
    throw std::runtime_error("TypeError: unsupported operand types for *");
}

PyValue PyValue::floor_div(const PyValue& rhs) const {
    if (is_numeric() && rhs.is_numeric()) {
        double r = rhs.to_float();
        if (r == 0.0) throw std::runtime_error("ZeroDivisionError: division by zero");
        if (type_ == Type::FLOAT || rhs.type_ == Type::FLOAT) {
            return PyValue(std::floor(to_float() / r));
        }
        return PyValue(as_int() / rhs.as_int());
    }
    throw std::runtime_error("TypeError: unsupported operand types for //");
}

PyValue PyValue::mod(const PyValue& rhs) const {
    if (type_ == Type::INT && rhs.type_ == Type::INT) {
        if (rhs.as_int() == 0) throw std::runtime_error("ZeroDivisionError: modulo by zero");
        return PyValue(as_int() % rhs.as_int());
    }
    if (is_numeric() && rhs.is_numeric()) {
        double r = rhs.to_float();
        if (r == 0.0) throw std::runtime_error("ZeroDivisionError: modulo by zero");
        return PyValue(std::fmod(to_float(), r));
    }
    throw std::runtime_error("TypeError: unsupported operand types for %");
}

PyValue PyValue::unary_neg() const {
    if (type_ == Type::INT) return PyValue(-as_int());
    if (type_ == Type::FLOAT) return PyValue(-as_float());
    throw std::runtime_error("TypeError: bad operand type for unary -");
}

PyValue PyValue::logical_not() const {
    return PyValue(!truthy());
}

// Comparison
bool PyValue::operator==(const PyValue& rhs) const {
    if (is_none_ && rhs.is_none_) return true;
    if (is_none_ || rhs.is_none_) return false;
    if (is_numeric() && rhs.is_numeric()) {
        if (type_ == Type::FLOAT || rhs.type_ == Type::FLOAT)
            return to_float() == rhs.to_float();
        return as_int() == rhs.as_int();
    }
    if (type_ == Type::BOOL && rhs.type_ == Type::BOOL)
        return as_bool() == rhs.as_bool();
    if (type_ == Type::STRING && rhs.type_ == Type::STRING)
        return as_string() == rhs.as_string();
    return false;
}

bool PyValue::operator!=(const PyValue& rhs) const {
    return !(*this == rhs);
}

bool PyValue::operator<(const PyValue& rhs) const {
    if (is_numeric() && rhs.is_numeric()) {
        if (type_ == Type::FLOAT || rhs.type_ == Type::FLOAT)
            return to_float() < rhs.to_float();
        return as_int() < rhs.as_int();
    }
    if (type_ == Type::STRING && rhs.type_ == Type::STRING)
        return as_string() < rhs.as_string();
    throw std::runtime_error("TypeError: unsupported comparison");
}

bool PyValue::operator>(const PyValue& rhs) const {
    return rhs < *this;
}

bool PyValue::operator<=(const PyValue& rhs) const {
    return !(rhs < *this);
}

bool PyValue::operator>=(const PyValue& rhs) const {
    return !(*this < rhs);
}

} // namespace mimo
