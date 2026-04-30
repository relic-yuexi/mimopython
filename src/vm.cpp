/**
 * @file vm.cpp
 * @brief Stack-based virtual machine - optimized with unified stack.
 *
 * Function calls use the same stack (no save/restore). This eliminates
 * the dominant overhead in recursive workloads.
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

PyValue Vm::load_var(uint32_t name_idx) {
    const std::string& name = code_->names[name_idx];
    // Search frames from most recent to oldest
    for (auto it = frames_.rbegin(); it != frames_.rend(); ++it) {
        auto found = it->locals.find(name);
        if (found != it->locals.end()) return found->second;
    }
    throw RuntimeError("NameError: name '" + name + "' is not defined", pc_);
}

void Vm::store_var(uint32_t name_idx, PyValue val) {
    const std::string& name = code_->names[name_idx];
    // Store in current frame
    frames_.back().locals[name] = std::move(val);
}

void Vm::execute(const CompiledCode& code) {
    code_ = &code;
    frames_.clear();
    frames_.push_back(CallFrame{});
    pc_ = 0;
    stack_.clear();
    run();
}

void Vm::run() {
    const auto& instrs = code_->instructions;
    const auto& consts = code_->constants;
    const auto& names = code_->names;

    while (pc_ < instrs.size()) {
        const auto& instr = instrs[pc_];
        pc_++;

        switch (instr.op) {
            case OpCode::LOAD_CONST:
                push(consts[instr.operand]);
                break;

            case OpCode::LOAD_NAME: {
                const std::string& name = names[instr.operand];
                try {
                    push(load_var(instr.operand));
                } catch (const RuntimeError&) {
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
                PyValue val = pop();
                store_var(instr.operand, std::move(val));
                break;
            }

            case OpCode::LOAD_FAST: {
                auto& frame = frames_.back();
                if (instr.operand < frame.fast_locals.size()) {
                    push(frame.fast_locals[instr.operand]);
                } else {
                    throw RuntimeError("VM: invalid local slot", pc_);
                }
                break;
            }

            case OpCode::STORE_FAST: {
                PyValue val = pop();
                auto& frame = frames_.back();
                if (instr.operand < frame.fast_locals.size()) {
                    frame.fast_locals[instr.operand] = std::move(val);
                } else {
                    throw RuntimeError("VM: invalid local slot", pc_);
                }
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
                if (!top().truthy()) {
                    pc_ = instr.operand;
                }
                break;
            }

            case OpCode::JUMP_IF_TRUE: {
                if (top().truthy()) {
                    pc_ = instr.operand;
                }
                break;
            }

            case OpCode::JUMP_ABSOLUTE:
                pc_ = instr.operand;
                break;

            case OpCode::CALL_FUNCTION: {
                uint32_t num_args = instr.operand;

                PyValue func_val = pop();
                if (func_val.type() != PyValue::Type::FUNCTION) {
                    throw RuntimeError("TypeError: object is not callable", pc_);
                }

                auto func = func_val.as_function();

                // Pop arguments
                std::vector<PyValue> args(num_args);
                for (int i = static_cast<int>(num_args) - 1; i >= 0; --i) {
                    args[i] = pop();
                }

                // Push new call frame
                CallFrame frame;
                frame.return_address = pc_;

                // Set up fast locals from function's slot names
                if (!func->local_slot_names.empty()) {
                    frame.fast_locals.resize(func->local_slot_names.size(), PyValue::none());
                    frame.local_slots.reserve(func->local_slot_names.size());
                    for (size_t i = 0; i < func->local_slot_names.size(); ++i) {
                        frame.local_slots[func->local_slot_names[i]] = static_cast<uint32_t>(i);
                    }
                }

                // Bind parameters (params are the first slots)
                for (size_t i = 0; i < func->params.size(); ++i) {
                    if (i < args.size()) {
                        if (i < frame.fast_locals.size()) {
                            frame.fast_locals[i] = std::move(args[i]);
                        }
                        frame.locals[func->params[i]] = frame.fast_locals[i];
                    }
                }

                frames_.push_back(std::move(frame));
                pc_ = func->entry_point;
                break;
            }

            case OpCode::RETURN_VALUE: {
                PyValue result = pop();
                if (frames_.size() > 1) {
                    uint32_t ret_addr = frames_.back().return_address;
                    frames_.pop_back();
                    pc_ = ret_addr;
                }
                push(std::move(result));
                break;
            }

            case OpCode::PRINT: {
                uint32_t num_args = instr.operand;
                std::vector<PyValue> args(num_args);
                for (int i = static_cast<int>(num_args) - 1; i >= 0; --i) {
                    args[i] = pop();
                }
                std::ostringstream oss;
                for (uint32_t i = 0; i < args.size(); ++i) {
                    if (i > 0) oss << " ";
                    oss << args[i].to_string();
                }
                std::string line = oss.str();
                output_.push_back(line);
                if (out_stream_) *out_stream_ << line << std::endl;
                else std::cout << line << std::endl;
                break;
            }

            case OpCode::MAKE_FUNCTION:
            case OpCode::FOR_ITER:
            case OpCode::GET_ITER:
            case OpCode::SETUP_LOOP:
            case OpCode::POP_BLOCK:
                break;

            case OpCode::BREAK_LOOP:
                if (frames_.empty()) throw RuntimeError("SyntaxError: 'break' outside loop", pc_);
                break;

            case OpCode::CONTINUE_LOOP:
                if (frames_.empty()) throw RuntimeError("SyntaxError: 'continue' outside loop", pc_);
                break;

            case OpCode::NOP:
                break;

            case OpCode::HALT:
                return;
        }
    }
}

} // namespace mimo
