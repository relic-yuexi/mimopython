/**
 * @file vm.h
 * @brief Stack-based virtual machine for executing bytecode.
 *
 * Optimized: unified stack with call frames. No save/restore per function call.
 */
#pragma once

#include "bytecode.h"
#include "value.h"

namespace mimo {
struct CompiledCode;
}

#include <vector>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <cstdint>
#include <iostream>
#include <filesystem>

namespace mimo {

class RuntimeError : public std::runtime_error {
public:
    uint32_t pc;
    RuntimeError(const std::string& msg, uint32_t p = 0)
        : std::runtime_error(msg), pc(p) {}
};

struct CallFrame {
    uint32_t return_address = 0;
    uint32_t base_pointer = 0;
    std::unordered_map<std::string, PyValue> locals;
    // Fast local access with small buffer optimization
    // For functions with <=4 params, avoid heap allocation
    static constexpr uint32_t INLINE_SLOTS = 4;
    PyValue inline_slots_[INLINE_SLOTS];
    std::vector<PyValue> fast_locals;  // only used if > INLINE_SLOTS
    std::unordered_map<std::string, uint32_t> local_slots;

    PyValue& get_fast_local(uint32_t idx) {
        if (fast_locals.empty()) return inline_slots_[idx];
        return fast_locals[idx];
    }

    void init_fast_locals(uint32_t count) {
        if (count <= INLINE_SLOTS) {
            for (uint32_t i = 0; i < INLINE_SLOTS; ++i) inline_slots_[i] = PyValue::none();
        } else {
            fast_locals.resize(count, PyValue::none());
        }
    }

    uint32_t fast_local_count() const {
        return fast_locals.empty() ? INLINE_SLOTS : static_cast<uint32_t>(fast_locals.size());
    }
};

class Vm {
public:
    Vm();
    void execute(CompiledCode& code);

private:
    std::vector<PyValue> stack_;
    std::vector<PyValue> globals_;  // indexed by name pool index
    std::vector<CallFrame> frames_;
    uint32_t pc_ = 0;
    CompiledCode* code_ = nullptr;

    // Module search path
    std::string script_dir_;
    // Already-imported modules (prevent re-import)
    std::unordered_map<std::string, std::unordered_map<std::string, PyValue>> imported_modules_;

    // Output capture
    std::vector<std::string> output_;
    std::ostream* out_stream_ = nullptr;

    CallFrame& current_frame();
    PyValue& top();
    PyValue pop();
    void push(PyValue val);

    PyValue load_var(uint32_t name_idx);
    void store_var(uint32_t name_idx, PyValue val);

    void run();

public:
    void set_output_stream(std::ostream& os) { out_stream_ = &os; }
    void set_script_dir(const std::string& dir) { script_dir_ = dir; }
    const std::vector<std::string>& output() const { return output_; }
};

} // namespace mimo
