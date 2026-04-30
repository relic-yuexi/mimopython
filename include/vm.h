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
    // Fast local access: indexed by slot number
    std::vector<PyValue> fast_locals;
    // Map name → slot index (for STORE_FAST/LOAD_FAST)
    std::unordered_map<std::string, uint32_t> local_slots;
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
