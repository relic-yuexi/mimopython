/**
 * @file value.h
 * @brief PyValue: runtime value representation using std::variant.
 *
 * Supports int, double, bool, string, None, and callable (function) types.
 * Provides implicit numeric conversions and comparison/arithmetic operations.
 */
#pragma once

#include <string>
#include <variant>
#include <vector>
#include <functional>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <cstdint>
#include <unordered_map>

namespace mimo {

// Forward declarations
class Vm;
struct PyFunction;

/**
 * @brief Runtime value in the interpreter.
 *
 * Backed by std::variant<int64_t, double, bool, std::string, std::shared_ptr<PyFunction>>.
 * A separate bool flag represents None.
 */
class PyValue {
public:
    enum class Type { INT, FLOAT, BOOL, STRING, NONE, FUNCTION };

private:
    using Variant = std::variant<int64_t, double, bool, std::string, std::shared_ptr<PyFunction>>;
    Variant data_;
    Type type_;
    bool is_none_ = false;

public:
    PyValue();
    explicit PyValue(int64_t v);
    explicit PyValue(double v);
    explicit PyValue(bool v);
    explicit PyValue(std::string v);
    explicit PyValue(std::shared_ptr<PyFunction> fn);
    static PyValue none();

    Type type() const { return type_; }
    bool is_none() const { return is_none_; }
    bool is_numeric() const { return type_ == Type::INT || type_ == Type::FLOAT; }

    int64_t as_int() const;
    double as_float() const;
    bool as_bool() const;
    const std::string& as_string() const;
    std::shared_ptr<PyFunction> as_function() const;

    double to_float() const;
    int64_t to_int() const;
    std::string to_string() const;

    bool truthy() const;

    PyValue operator+(const PyValue& rhs) const;
    PyValue operator-(const PyValue& rhs) const;
    PyValue operator*(const PyValue& rhs) const;
    PyValue floor_div(const PyValue& rhs) const;
    PyValue mod(const PyValue& rhs) const;
    PyValue unary_neg() const;
    PyValue logical_not() const;

    bool operator==(const PyValue& rhs) const;
    bool operator!=(const PyValue& rhs) const;
    bool operator<(const PyValue& rhs) const;
    bool operator>(const PyValue& rhs) const;
    bool operator<=(const PyValue& rhs) const;
    bool operator>=(const PyValue& rhs) const;
};

struct PyFunction {
    std::string name;
    std::vector<std::string> params;
    uint32_t entry_point = 0;
    std::unordered_map<std::string, PyValue> closure;
    std::vector<std::string> local_slot_names;
    // JIT: cached native function pointer
    using NativeFunc = int64_t(*)(int64_t);
    NativeFunc native_func = nullptr;
    uint32_t call_count = 0;
};

} // namespace mimo
