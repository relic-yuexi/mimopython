/**
 * @file vm.cpp
 * @brief Stack-based virtual machine implementation.
 *
 * Call frame stack manages local scopes. Each CALL_FUNCTION pushes a frame
 * with return address and parameters. RETURN_VALUE pops the frame and
 * pushes the result.
 */
#include "vm.h"
#include "compiler.h"
#include <sstream>
#include <iostream>
#include <algorithm>

namespace mimo {

Vm::Vm() = default;

CallFrame& Vm::current_frame() {
    return frames_.back();
}

PyValue& Vm::top() {
    if (stack_.empty()) throw RuntimeError("VM: stack underflow", pc_);
    return stack_.back();
}

PyValue Vm::pop() {
    if (stack_.empty()) throw RuntimeError("VM: stack underflow", pc_);
    PyValue val = std::move(stack_.back());
    stack_.pop_back();
    return val;
}

void Vm::push(PyValue val) {
    stack_.push_back(std::move(val));
}

PyValue Vm::load_var(const std::string& name) {
    for (auto it = frames_.rbegin(); it != frames_.rend(); ++it) {
        auto found = it->locals.find(name);
        if (found != it->locals.end()) return found->second;
        auto cf = it->closure.find(name);
        if (cf != it->closure.end()) return cf->second;
    }
    throw RuntimeError("NameError: name '" + name + "' is not defined", pc_);
}

void Vm::store_var(const std::string& name, PyValue val) {
    // Always store in current frame (creates local variable in functions)
    current_frame().locals[name] = std::move(val);
}

bool Vm::has_var(const std::string& name) const {
    for (auto it = frames_.rbegin(); it != frames_.rend(); ++it) {
        if (it->locals.count(name) || it->closure.count(name)) return true;
    }
    return false;
}

void Vm::execute(const CompiledCode& code) {
    code_ = &code;
    frames_.clear();
    frames_.push_back(CallFrame{});
    pc_ = 0;
    returned_ = false;
    run();
}

void Vm::execute_function(const CompiledCode& code, uint32_t entry,
                          const std::unordered_map<std::string, PyValue>& closure,
                          std::vector<PyValue> args,
                          const std::vector<std::string>& param_names) {
    // Save current VM state
    auto saved_code = code_;
    auto saved_pc = pc_;
    auto saved_stack = std::move(stack_);
    auto saved_frames = std::move(frames_);
    auto saved_loops = std::move(loop_stack_);

    code_ = &code;
    frames_.clear();
    CallFrame frame;
    frame.closure = closure;
    frame.return_address = 0;

    // Bind parameters as locals
    for (size_t i = 0; i < param_names.size(); ++i) {
        if (i < args.size()) {
            frame.locals[param_names[i]] = std::move(args[i]);
        } else {
            frame.locals[param_names[i]] = PyValue::none();
        }
    }
    frames_.push_back(std::move(frame));
    pc_ = entry;
    stack_.clear();
    loop_stack_.clear();

    run();

    // Result is on top of stack
    PyValue result = PyValue::none();
    if (!stack_.empty()) {
        result = pop();
    }

    // Restore state
    code_ = saved_code;
    pc_ = saved_pc;
    stack_ = std::move(saved_stack);
    frames_ = std::move(saved_frames);
    loop_stack_ = std::move(saved_loops);

    // Push result onto caller's stack
    push(std::move(result));
}

void Vm::run() {
    while (pc_ < code_->instructions.size()) {
        auto& instr = code_->instructions[pc_];
        pc_++;

        switch (instr.op) {
            case OpCode::LOAD_CONST:
                push(code_->constants[instr.operand]);
                break;

            case OpCode::LOAD_NAME: {
                const std::string& name = code_->names[instr.operand];
                try {
                    push(load_var(name));
                } catch (const RuntimeError&) {
                    // Built-in: range
                    if (name == "range") {
                        auto fn = std::make_shared<PyFunction>();
                        fn->name = "range";
                        push(PyValue(fn));
                    } else {
                        throw;
                    }
                }
                break;
            }

            case OpCode::STORE_NAME: {
                const std::string& name = code_->names[instr.operand];
                PyValue val = pop();
                store_var(name, std::move(val));
                break;
            }

            case OpCode::POP_TOP:
                pop();
                break;

            case OpCode::DUP_TOP:
                push(top());
                break;

            case OpCode::BINARY_ADD: {
                PyValue rhs = pop();
                PyValue lhs = pop();
                push(lhs + rhs);
                break;
            }

            case OpCode::BINARY_SUB: {
                PyValue rhs = pop();
                PyValue lhs = pop();
                push(lhs - rhs);
                break;
            }

            case OpCode::BINARY_MUL: {
                PyValue rhs = pop();
                PyValue lhs = pop();
                push(lhs * rhs);
                break;
            }

            case OpCode::BINARY_FLOOR_DIV: {
                PyValue rhs = pop();
                PyValue lhs = pop();
                push(lhs.floor_div(rhs));
                break;
            }

            case OpCode::BINARY_MOD: {
                PyValue rhs = pop();
                PyValue lhs = pop();
                push(lhs.mod(rhs));
                break;
            }

            case OpCode::UNARY_NEG: {
                PyValue val = pop();
                push(val.unary_neg());
                break;
            }

            case OpCode::UNARY_NOT: {
                PyValue val = pop();
                push(val.logical_not());
                break;
            }

            case OpCode::COMPARE_EQ: {
                PyValue rhs = pop();
                PyValue lhs = pop();
                push(PyValue(lhs == rhs));
                break;
            }

            case OpCode::COMPARE_NEQ: {
                PyValue rhs = pop();
                PyValue lhs = pop();
                push(PyValue(lhs != rhs));
                break;
            }

            case OpCode::COMPARE_LT: {
                PyValue rhs = pop();
                PyValue lhs = pop();
                push(PyValue(lhs < rhs));
                break;
            }

            case OpCode::COMPARE_GT: {
                PyValue rhs = pop();
                PyValue lhs = pop();
                push(PyValue(lhs > rhs));
                break;
            }

            case OpCode::COMPARE_LTE: {
                PyValue rhs = pop();
                PyValue lhs = pop();
                push(PyValue(lhs <= rhs));
                break;
            }

            case OpCode::COMPARE_GTE: {
                PyValue rhs = pop();
                PyValue lhs = pop();
                push(PyValue(lhs >= rhs));
                break;
            }

            case OpCode::JUMP_IF_FALSE: {
                PyValue val = top();
                if (!val.truthy()) {
                    pc_ = instr.operand;
                }
                break;
            }

            case OpCode::JUMP_IF_TRUE: {
                PyValue val = top();
                if (val.truthy()) {
                    pc_ = instr.operand;
                }
                break;
            }

            case OpCode::JUMP_ABSOLUTE:
                pc_ = instr.operand;
                break;

            case OpCode::CALL_FUNCTION: {
                uint32_t num_args = instr.operand;

                // Stack: [arg0, arg1, ..., argN-1, function]
                PyValue func_val = pop();
                if (func_val.type() != PyValue::Type::FUNCTION) {
                    throw RuntimeError("TypeError: object is not callable", pc_);
                }

                auto func = func_val.as_function();
                std::vector<PyValue> args;
                args.reserve(num_args);
                for (uint32_t i = 0; i < num_args; ++i) {
                    args.push_back(pop());
                }
                std::reverse(args.begin(), args.end());

                // Set up new call frame
                CallFrame new_frame;
                new_frame.return_address = pc_;
                for (size_t i = 0; i < func->params.size(); ++i) {
                    if (i < args.size()) {
                        new_frame.locals[func->params[i]] = std::move(args[i]);
                    } else {
                        new_frame.locals[func->params[i]] = PyValue::none();
                    }
                }

                // Save stack and switch to function
                auto saved_stack = std::move(stack_);
                auto saved_loops = std::move(loop_stack_);

                frames_.push_back(std::move(new_frame));
                uint32_t saved_pc = pc_;
                pc_ = func->entry_point;
                stack_.clear();
                loop_stack_.clear();

                // Execute function body
                run();

                // Get result
                PyValue result = PyValue::none();
                if (!stack_.empty()) {
                    result = pop();
                }

                // Restore
                pc_ = saved_pc;
                stack_ = std::move(saved_stack);
                loop_stack_ = std::move(saved_loops);
                frames_.pop_back();

                push(std::move(result));
                break;
            }

            case OpCode::RETURN_VALUE:
                // Return is handled by simply returning from run().
                // The caller will pick up the result from the stack.
                return;

            case OpCode::PRINT: {
                uint32_t num_args = instr.operand;
                std::vector<PyValue> args;
                for (uint32_t i = 0; i < num_args; ++i) {
                    args.push_back(pop());
                }
                // Reverse since we popped from top
                std::reverse(args.begin(), args.end());
                std::ostringstream oss;
                for (uint32_t i = 0; i < args.size(); ++i) {
                    if (i > 0) oss << " ";
                    oss << args[i].to_string();
                }
                std::string line = oss.str();
                output_.push_back(line);
                if (out_stream_) {
                    *out_stream_ << line << std::endl;
                } else {
                    std::cout << line << std::endl;
                }
                break;
            }

            case OpCode::MAKE_FUNCTION:
                break;

            case OpCode::FOR_ITER:
            case OpCode::GET_ITER:
            case OpCode::SETUP_LOOP:
            case OpCode::POP_BLOCK:
                break;

            case OpCode::BREAK_LOOP:
                if (loop_stack_.empty()) {
                    throw RuntimeError("SyntaxError: 'break' outside loop", pc_);
                }
                pc_ = loop_stack_.back().break_target;
                break;

            case OpCode::CONTINUE_LOOP:
                if (loop_stack_.empty()) {
                    throw RuntimeError("SyntaxError: 'continue' outside loop", pc_);
                }
                pc_ = loop_stack_.back().continue_target;
                break;

            case OpCode::NOP:
                break;

            case OpCode::HALT:
                return;
        }
    }
}

} // namespace mimo
