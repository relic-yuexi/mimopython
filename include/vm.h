/**
 * @file vm.h
 * @brief Stack-based virtual machine for executing bytecode.
 *
 * Implements an operand stack, call frame stack, and scope chain.
 * Each call frame has its own local variable namespace.
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

namespace mimo {

class RuntimeError : public std::runtime_error {
public:
    uint32_t pc;
    RuntimeError(const std::string& msg, uint32_t p = 0)
        : std::runtime_error(msg), pc(p) {}
};

struct CallFrame {
    std::unordered_map<std::string, PyValue> locals;
    uint32_t return_address = 0;
    // For function frames: the function's closure
    std::unordered_map<std::string, PyValue> closure;
};

struct LoopFrame {
    uint32_t break_target;
    uint32_t continue_target;
};

class Vm {
public:
    Vm();
    void execute(const CompiledCode& code);
    // Execute with a pre-loaded environment (for function calls)
    void execute_function(const CompiledCode& code, uint32_t entry,
                          const std::unordered_map<std::string, PyValue>& closure,
                          std::vector<PyValue> args,
                          const std::vector<std::string>& param_names);
    PyValue last_result() const { return last_result_; }

private:
    std::vector<PyValue> stack_;
    std::vector<CallFrame> frames_;
    std::vector<LoopFrame> loop_stack_;
    uint32_t pc_ = 0;
    const CompiledCode* code_ = nullptr;
    PyValue last_result_;
    bool returned_ = false;

    // Output capture
    std::vector<std::string> output_;
    std::ostream* out_stream_ = nullptr;

    CallFrame& current_frame();
    PyValue& top();
    PyValue pop();
    void push(PyValue val);

    // Variable resolution: local -> closure -> global
    PyValue load_var(const std::string& name);
    void store_var(const std::string& name, PyValue val);
    bool has_var(const std::string& name) const;

    void run();

    // Public interface for tests
public:
    void set_output_stream(std::ostream& os) { out_stream_ = &os; }
    const std::vector<std::string>& output() const { return output_; }
};

} // namespace mimo
