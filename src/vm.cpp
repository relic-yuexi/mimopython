/**
 * @file vm.cpp
 * @brief Stack-based virtual machine - optimized hot path.
 *
 * Key optimizations:
 * - Unified stack (no save/restore per call)
 * - Indexed local/global variable access
 * - Inlined hot instructions
 * - Reduced function call overhead in dispatch loop
 */
#include "vm.h"
#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace mimo {

Vm::Vm() = default;

void Vm::execute(CompiledCode& code) {
    code_ = &code;
    frames_.clear();
    frames_.push_back(CallFrame{});
    globals_.clear();
    globals_.resize(code.names.size());
    pc_ = 0;
    stack_.clear();
    run();
}

void Vm::run() {
    const auto* __restrict instrs = code_->instructions.data();
    const auto* __restrict consts = code_->constants.data();
    auto* names = code_->names.data();
    uint32_t num_instrs = static_cast<uint32_t>(code_->instructions.size());
    uint32_t num_names = static_cast<uint32_t>(code_->names.size());

    // Cache frequently accessed frame
    CallFrame* frame = &frames_.back();

    while (pc_ < num_instrs) {
        const auto& instr = instrs[pc_];
        pc_++;

        switch (instr.op) {
            case OpCode::LOAD_CONST:
                stack_.push_back(consts[instr.operand]);
                break;

            case OpCode::LOAD_NAME: {
                if (instr.operand < globals_.size()) {
                    stack_.push_back(globals_[instr.operand]);
                } else if (instr.operand < num_names && names[instr.operand] == std::string_view("range")) {
                    auto fn = std::make_shared<PyFunction>();
                    fn->name = "range";
                    stack_.push_back(PyValue(fn));
                } else {
                    throw RuntimeError("NameError: name '" + std::string(names[instr.operand]) + "' is not defined", pc_);
                }
                break;
            }

            case OpCode::STORE_NAME: {
                PyValue val = std::move(stack_.back());
                stack_.pop_back();
                if (instr.operand >= globals_.size()) {
                    globals_.resize(instr.operand + 1);
                }
                globals_[instr.operand] = std::move(val);
                break;
            }

            case OpCode::LOAD_FAST: {
                if (instr.operand < frame->fast_locals.size()) {
                    stack_.push_back(frame->fast_locals[instr.operand]);
                } else {
                    throw RuntimeError("VM: invalid local slot", pc_);
                }
                break;
            }

            case OpCode::STORE_FAST: {
                PyValue val = std::move(stack_.back());
                stack_.pop_back();
                if (instr.operand < frame->fast_locals.size()) {
                    frame->fast_locals[instr.operand] = std::move(val);
                } else {
                    throw RuntimeError("VM: invalid local slot", pc_);
                }
                break;
            }

            case OpCode::POP_TOP:
                stack_.pop_back();
                break;

            case OpCode::DUP_TOP:
                stack_.push_back(stack_.back());
                break;

            // Hot path: integer arithmetic (most common case)
            case OpCode::BINARY_ADD: {
                PyValue rhs = std::move(stack_.back()); stack_.pop_back();
                PyValue& lhs = stack_.back();
                if ((lhs.type() == PyValue::Type::INT || lhs.type() == PyValue::Type::BOOL) &&
                    (rhs.type() == PyValue::Type::INT || rhs.type() == PyValue::Type::BOOL)) {
                    lhs = PyValue(lhs.to_int() + rhs.to_int());
                } else if (lhs.is_numeric() && rhs.is_numeric()) {
                    lhs = PyValue(lhs.to_float() + rhs.to_float());
                } else if (lhs.type() == PyValue::Type::STRING || rhs.type() == PyValue::Type::STRING) {
                    lhs = PyValue(lhs.to_string() + rhs.to_string());
                } else {
                    throw RuntimeError("TypeError: unsupported operand types for +", pc_);
                }
                break;
            }

            case OpCode::BINARY_SUB: {
                PyValue rhs = std::move(stack_.back()); stack_.pop_back();
                PyValue& lhs = stack_.back();
                if (lhs.type() == PyValue::Type::INT && rhs.type() == PyValue::Type::INT) {
                    lhs = PyValue(lhs.as_int() - rhs.as_int());
                } else if (lhs.is_numeric() && rhs.is_numeric()) {
                    lhs = PyValue(lhs.to_float() - rhs.to_float());
                } else {
                    throw RuntimeError("TypeError: unsupported operand types for -", pc_);
                }
                break;
            }

            case OpCode::BINARY_MUL: {
                PyValue rhs = std::move(stack_.back()); stack_.pop_back();
                PyValue& lhs = stack_.back();
                if (lhs.type() == PyValue::Type::INT && rhs.type() == PyValue::Type::INT) {
                    lhs = PyValue(lhs.as_int() * rhs.as_int());
                } else if (lhs.is_numeric() && rhs.is_numeric()) {
                    lhs = PyValue(lhs.to_float() * rhs.to_float());
                } else if (lhs.type() == PyValue::Type::STRING && rhs.type() == PyValue::Type::INT) {
                    std::string result;
                    int64_t n = rhs.as_int();
                    const std::string& s = lhs.as_string();
                    for (int64_t i = 0; i < n; ++i) result += s;
                    lhs = PyValue(std::move(result));
                } else {
                    throw RuntimeError("TypeError: unsupported operand types for *", pc_);
                }
                break;
            }

            case OpCode::BINARY_FLOOR_DIV: {
                PyValue rhs = std::move(stack_.back()); stack_.pop_back();
                PyValue& lhs = stack_.back();
                if (lhs.type() == PyValue::Type::INT && rhs.type() == PyValue::Type::INT) {
                    if (rhs.as_int() == 0) throw RuntimeError("ZeroDivisionError: division by zero", pc_);
                    lhs = PyValue(lhs.as_int() / rhs.as_int());
                } else if (lhs.is_numeric() && rhs.is_numeric()) {
                    double r = rhs.to_float();
                    if (r == 0.0) throw RuntimeError("ZeroDivisionError: division by zero", pc_);
                    lhs = PyValue(std::floor(lhs.to_float() / r));
                } else {
                    throw RuntimeError("TypeError: unsupported operand types for //", pc_);
                }
                break;
            }

            case OpCode::BINARY_MOD: {
                PyValue rhs = std::move(stack_.back()); stack_.pop_back();
                PyValue& lhs = stack_.back();
                if (lhs.type() == PyValue::Type::INT && rhs.type() == PyValue::Type::INT) {
                    if (rhs.as_int() == 0) throw RuntimeError("ZeroDivisionError: modulo by zero", pc_);
                    lhs = PyValue(lhs.as_int() % rhs.as_int());
                } else if (lhs.is_numeric() && rhs.is_numeric()) {
                    double r = rhs.to_float();
                    if (r == 0.0) throw RuntimeError("ZeroDivisionError: modulo by zero", pc_);
                    lhs = PyValue(std::fmod(lhs.to_float(), r));
                } else {
                    throw RuntimeError("TypeError: unsupported operand types for %", pc_);
                }
                break;
            }

            case OpCode::UNARY_NEG: {
                PyValue& val = stack_.back();
                if (val.type() == PyValue::Type::INT) {
                    val = PyValue(-val.as_int());
                } else if (val.type() == PyValue::Type::FLOAT) {
                    val = PyValue(-val.as_float());
                } else {
                    throw RuntimeError("TypeError: bad operand type for unary -", pc_);
                }
                break;
            }

            case OpCode::UNARY_NOT: {
                PyValue& val = stack_.back();
                val = PyValue(!val.truthy());
                break;
            }

            // Hot path: integer comparison
            case OpCode::COMPARE_LT: {
                PyValue rhs = std::move(stack_.back()); stack_.pop_back();
                PyValue& lhs = stack_.back();
                if (lhs.type() == PyValue::Type::INT && rhs.type() == PyValue::Type::INT) {
                    lhs = PyValue(lhs.as_int() < rhs.as_int());
                } else if (lhs.is_numeric() && rhs.is_numeric()) {
                    lhs = PyValue(lhs.to_float() < rhs.to_float());
                } else if (lhs.type() == PyValue::Type::STRING && rhs.type() == PyValue::Type::STRING) {
                    lhs = PyValue(lhs.as_string() < rhs.as_string());
                } else {
                    throw RuntimeError("TypeError: unsupported comparison", pc_);
                }
                break;
            }

            case OpCode::COMPARE_GT: {
                PyValue rhs = std::move(stack_.back()); stack_.pop_back();
                PyValue& lhs = stack_.back();
                if (lhs.type() == PyValue::Type::INT && rhs.type() == PyValue::Type::INT) {
                    lhs = PyValue(lhs.as_int() > rhs.as_int());
                } else if (lhs.is_numeric() && rhs.is_numeric()) {
                    lhs = PyValue(lhs.to_float() > rhs.to_float());
                } else {
                    lhs = PyValue(lhs > rhs);
                }
                break;
            }

            case OpCode::COMPARE_EQ: {
                PyValue rhs = std::move(stack_.back()); stack_.pop_back();
                PyValue& lhs = stack_.back();
                if (lhs.type() == PyValue::Type::INT && rhs.type() == PyValue::Type::INT) {
                    lhs = PyValue(lhs.as_int() == rhs.as_int());
                } else {
                    lhs = PyValue(lhs == rhs);
                }
                break;
            }

            case OpCode::COMPARE_NEQ: {
                PyValue rhs = std::move(stack_.back()); stack_.pop_back();
                PyValue& lhs = stack_.back();
                lhs = PyValue(lhs != rhs);
                break;
            }

            case OpCode::COMPARE_LTE: {
                PyValue rhs = std::move(stack_.back()); stack_.pop_back();
                PyValue& lhs = stack_.back();
                if (lhs.type() == PyValue::Type::INT && rhs.type() == PyValue::Type::INT) {
                    lhs = PyValue(lhs.as_int() <= rhs.as_int());
                } else if (lhs.is_numeric() && rhs.is_numeric()) {
                    lhs = PyValue(lhs.to_float() <= rhs.to_float());
                } else {
                    lhs = PyValue(lhs <= rhs);
                }
                break;
            }

            case OpCode::COMPARE_GTE: {
                PyValue rhs = std::move(stack_.back()); stack_.pop_back();
                PyValue& lhs = stack_.back();
                lhs = PyValue(lhs >= rhs);
                break;
            }

            case OpCode::JUMP_IF_FALSE: {
                if (!stack_.back().truthy()) {
                    pc_ = instr.operand;
                }
                break;
            }

            case OpCode::JUMP_IF_TRUE: {
                if (stack_.back().truthy()) {
                    pc_ = instr.operand;
                }
                break;
            }

            case OpCode::JUMP_ABSOLUTE:
                pc_ = instr.operand;
                break;

            case OpCode::CALL_FUNCTION: {
                uint32_t num_args = instr.operand;

                PyValue func_val = std::move(stack_.back()); stack_.pop_back();
                if (func_val.type() != PyValue::Type::FUNCTION) {
                    throw RuntimeError("TypeError: object is not callable", pc_);
                }

                auto func = func_val.as_function();

                // Pop arguments
                std::vector<PyValue> args(num_args);
                for (int i = static_cast<int>(num_args) - 1; i >= 0; --i) {
                    args[i] = std::move(stack_.back()); stack_.pop_back();
                }

                // Push new call frame
                CallFrame new_frame;
                new_frame.return_address = pc_;

                // Set up fast locals
                if (!func->local_slot_names.empty()) {
                    new_frame.fast_locals.resize(func->local_slot_names.size(), PyValue::none());
                    new_frame.local_slots.reserve(func->local_slot_names.size());
                    for (size_t i = 0; i < func->local_slot_names.size(); ++i) {
                        new_frame.local_slots[func->local_slot_names[i]] = static_cast<uint32_t>(i);
                    }
                }

                // Bind parameters
                for (size_t i = 0; i < func->params.size() && i < args.size(); ++i) {
                    if (i < new_frame.fast_locals.size()) {
                        new_frame.fast_locals[i] = std::move(args[i]);
                    }
                }

                frames_.push_back(std::move(new_frame));
                frame = &frames_.back();
                pc_ = func->entry_point;
                break;
            }

            case OpCode::RETURN_VALUE: {
                PyValue result = std::move(stack_.back()); stack_.pop_back();
                if (frames_.size() > 1) {
                    uint32_t ret_addr = frames_.back().return_address;
                    frames_.pop_back();
                    frame = &frames_.back();
                    pc_ = ret_addr;
                }
                stack_.push_back(std::move(result));
                break;
            }

            case OpCode::PRINT: {
                uint32_t num_args = instr.operand;
                std::vector<PyValue> args(num_args);
                for (int i = static_cast<int>(num_args) - 1; i >= 0; --i) {
                    args[i] = std::move(stack_.back()); stack_.pop_back();
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

            case OpCode::IMPORT_NAME: {
                const std::string& mod_name = names[instr.operand];

                // Check if already imported
                if (imported_modules_.count(mod_name)) {
                    for (auto& [k, v] : imported_modules_[mod_name]) {
                        uint32_t idx = code_->find_or_add_name(k);
                        if (idx >= globals_.size()) globals_.resize(idx + 1);
                        globals_[idx] = v;
                    }
                    names = code_->names.data();
                    num_names = static_cast<uint32_t>(code_->names.size());
                    break;
                }

                // Find the .py file
                std::string filename = mod_name + ".py";
                std::string filepath = script_dir_.empty() ? filename : script_dir_ + "/" + filename;

                std::ifstream file(filepath);
                if (!file.is_open()) {
                    throw RuntimeError("ModuleNotFoundError: No module named '" + mod_name + "'", pc_);
                }

                std::ostringstream ss;
                ss << file.rdbuf();
                std::string source = ss.str();
                if (!source.empty() && source.back() != '\n') source += '\n';

                // Compile the module
                mimo::Lexer mod_lexer(source);
                auto mod_tokens = mod_lexer.tokenize();
                mimo::Parser mod_parser(std::move(mod_tokens));
                auto mod_ast = mod_parser.parse();
                mimo::Compiler mod_compiler;
                auto mod_code = mod_compiler.compile(mod_ast);

                // Merge module's code into parent's code
                uint32_t instr_offset = static_cast<uint32_t>(code_->instructions.size());
                uint32_t const_offset = static_cast<uint32_t>(code_->constants.size());

                // Build name mapping: module name index → parent name index
                // Deduplicate names that already exist in parent
                std::vector<uint32_t> name_map(mod_code.names.size());
                for (size_t i = 0; i < mod_code.names.size(); ++i) {
                    name_map[i] = code_->find_or_add_name(mod_code.names[i]);
                }

                // Append constants (fix function entry_points)
                for (auto& c : mod_code.constants) {
                    if (c.type() == PyValue::Type::FUNCTION) {
                        auto fn = c.as_function();
                        fn->entry_point += instr_offset;
                    }
                    code_->constants.push_back(std::move(c));
                }

                // Append instructions (adjust operands using name_map)
                for (auto& mod_instr : mod_code.instructions) {
                    Instruction adjusted = mod_instr;
                    switch (mod_instr.op) {
                        case OpCode::LOAD_CONST:
                        case OpCode::IMPORT_NAME:
                            adjusted.operand += const_offset;
                            break;
                        case OpCode::LOAD_NAME:
                        case OpCode::STORE_NAME:
                        case OpCode::MAKE_FUNCTION:
                            adjusted.operand = name_map[adjusted.operand];
                            break;
                        case OpCode::JUMP_IF_FALSE:
                        case OpCode::JUMP_IF_TRUE:
                        case OpCode::JUMP_ABSOLUTE:
                            adjusted.operand += instr_offset;
                            break;
                        case OpCode::CALL_FUNCTION:
                        case OpCode::PRINT:
                        case OpCode::RETURN_VALUE:
                            break;
                        default:
                            break;
                    }
                    code_->instructions.push_back(adjusted);
                }

                // Replace module's HALT with a jump back to parent's next instruction
                uint32_t return_addr = pc_; // parent's next instruction after IMPORT
                code_->instructions[instr_offset + mod_code.instructions.size() - 1] =
                    Instruction(OpCode::JUMP_ABSOLUTE, return_addr);

                // Refresh cached pointers
                instrs = code_->instructions.data();
                consts = code_->constants.data();
                names = code_->names.data();
                num_instrs = static_cast<uint32_t>(code_->instructions.size());
                num_names = static_cast<uint32_t>(code_->names.size());

                // Jump to module's code
                pc_ = instr_offset;

                break;
            }

            case OpCode::MAKE_FUNCTION:
                break;

            case OpCode::FOR_ITER:
            case OpCode::GET_ITER:
            case OpCode::SETUP_LOOP:
            case OpCode::POP_BLOCK:
            case OpCode::BREAK_LOOP:
            case OpCode::CONTINUE_LOOP:
            case OpCode::NOP:
                break;

            case OpCode::HALT:
                return;
        }
    }
}

} // namespace mimo
