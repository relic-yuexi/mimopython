/**
 * @file vm.cpp
 * @brief Stack-based virtual machine - optimized with computed goto.
 *
 * Key optimizations:
 * - Computed goto dispatch (GCC extension, 10-20% faster than switch)
 * - Unified stack (no save/restore per call)
 * - Indexed local/global variable access
 * - Inlined hot instructions with integer fast paths
 * - Minimal stack manipulation overhead
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
    auto* __restrict instrs = code_->instructions.data();
    auto* __restrict consts = code_->constants.data();
    auto* names = code_->names.data();
    uint32_t num_instrs = static_cast<uint32_t>(code_->instructions.size());
    uint32_t num_names = static_cast<uint32_t>(code_->names.size());

    CallFrame* frame = &frames_.back();

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    // Computed goto dispatch table
    #define DISPATCH() goto *dispatch_table[static_cast<uint8_t>(instr.op)]
    #define CASE(op) DO_##op
    #define NEXT() \
        instr = instrs[pc_]; \
        pc_++; \
        DISPATCH()

    static const void* dispatch_table[] = {
        &&DO_LOAD_CONST, &&DO_LOAD_NAME, &&DO_STORE_NAME,
        &&DO_LOAD_FAST, &&DO_STORE_FAST,
        &&DO_POP_TOP, &&DO_BINARY_ADD, &&DO_BINARY_SUB, &&DO_BINARY_MUL,
        &&DO_BINARY_FLOOR_DIV, &&DO_BINARY_MOD,
        &&DO_UNARY_NEG, &&DO_UNARY_NOT,
        &&DO_COMPARE_EQ, &&DO_COMPARE_NEQ, &&DO_COMPARE_LT,
        &&DO_COMPARE_GT, &&DO_COMPARE_LTE, &&DO_COMPARE_GTE,
        &&DO_JUMP_IF_FALSE, &&DO_JUMP_IF_TRUE, &&DO_JUMP_ABSOLUTE,
        &&DO_CALL_FUNCTION, &&DO_RETURN_VALUE,
        &&DO_PRINT, &&DO_MAKE_FUNCTION, &&DO_IMPORT_NAME,
        &&DO_FOR_ITER, &&DO_GET_ITER, &&DO_SETUP_LOOP, &&DO_POP_BLOCK,
        &&DO_BREAK_LOOP, &&DO_CONTINUE_LOOP, &&DO_DUP_TOP, &&DO_NOP,
        &&DO_HALT
    };

    auto instr = instrs[pc_];
    pc_++;
    DISPATCH();

    CASE(LOAD_CONST): {
        stack_.push_back(consts[instr.operand]);
        NEXT();
    }

    CASE(LOAD_NAME): {
        if (instr.operand < globals_.size()) {
            stack_.push_back(globals_[instr.operand]);
        } else if (instr.operand < num_names && names[instr.operand] == std::string_view("range")) {
            auto fn = std::make_shared<PyFunction>();
            fn->name = "range";
            stack_.push_back(PyValue(fn));
        } else {
            throw RuntimeError("NameError: name '" + std::string(names[instr.operand]) + "' is not defined", pc_);
        }
        NEXT();
    }

    CASE(STORE_NAME): {
        PyValue val = std::move(stack_.back());
        stack_.pop_back();
        if (instr.operand >= globals_.size()) globals_.resize(instr.operand + 1);
        globals_[instr.operand] = std::move(val);
        NEXT();
    }

    CASE(LOAD_FAST): {
        stack_.push_back(frame->get_fast_local(instr.operand));
        NEXT();
    }

    CASE(STORE_FAST): {
        frame->get_fast_local(instr.operand) = std::move(stack_.back());
        stack_.pop_back();
        NEXT();
    }

    CASE(POP_TOP): {
        stack_.pop_back();
        NEXT();
    }

    CASE(DUP_TOP): {
        stack_.push_back(stack_.back());
        NEXT();
    }

    CASE(BINARY_ADD): {
        PyValue rhs = std::move(stack_.back()); stack_.pop_back();
        PyValue& lhs = stack_.back();
        if ((lhs.type() == PyValue::Type::INT || lhs.type() == PyValue::Type::BOOL) &&
            (rhs.type() == PyValue::Type::INT || rhs.type() == PyValue::Type::BOOL)) {
            lhs = PyValue(lhs.to_int() + rhs.to_int());
        } else if (lhs.is_numeric() && rhs.is_numeric()) {
            lhs = PyValue(lhs.to_float() + rhs.to_float());
        } else if (lhs.type() == PyValue::Type::STRING || rhs.type() == PyValue::Type::STRING) {
            if (lhs.type() == PyValue::Type::STRING) {
                // Move string out of lhs to avoid O(n) copy
                std::string result = lhs.move_string();
                if (rhs.type() == PyValue::Type::STRING) {
                    result += rhs.as_string();
                } else {
                    result += rhs.to_string();
                }
                lhs = PyValue(std::move(result));
            } else {
                lhs = PyValue(lhs.to_string() + rhs.to_string());
            }
        } else {
            throw RuntimeError("TypeError: unsupported operand types for +", pc_);
        }
        NEXT();
    }

    CASE(BINARY_SUB): {
        PyValue rhs = std::move(stack_.back()); stack_.pop_back();
        PyValue& lhs = stack_.back();
        if ((lhs.type() == PyValue::Type::INT || lhs.type() == PyValue::Type::BOOL) &&
            (rhs.type() == PyValue::Type::INT || rhs.type() == PyValue::Type::BOOL)) {
            lhs = PyValue(lhs.to_int() - rhs.to_int());
        } else if (lhs.is_numeric() && rhs.is_numeric()) {
            lhs = PyValue(lhs.to_float() - rhs.to_float());
        } else {
            throw RuntimeError("TypeError: unsupported operand types for -", pc_);
        }
        NEXT();
    }

    CASE(BINARY_MUL): {
        PyValue rhs = std::move(stack_.back()); stack_.pop_back();
        PyValue& lhs = stack_.back();
        if ((lhs.type() == PyValue::Type::INT || lhs.type() == PyValue::Type::BOOL) &&
            (rhs.type() == PyValue::Type::INT || rhs.type() == PyValue::Type::BOOL)) {
            lhs = PyValue(lhs.to_int() * rhs.to_int());
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
        NEXT();
    }

    CASE(BINARY_FLOOR_DIV): {
        PyValue rhs = std::move(stack_.back()); stack_.pop_back();
        PyValue& lhs = stack_.back();
        if ((lhs.type() == PyValue::Type::INT || lhs.type() == PyValue::Type::BOOL) &&
            (rhs.type() == PyValue::Type::INT || rhs.type() == PyValue::Type::BOOL)) {
            int64_t r = rhs.to_int();
            if (r == 0) throw RuntimeError("ZeroDivisionError: division by zero", pc_);
            lhs = PyValue(lhs.to_int() / r);
        } else if (lhs.is_numeric() && rhs.is_numeric()) {
            double r = rhs.to_float();
            if (r == 0.0) throw RuntimeError("ZeroDivisionError: division by zero", pc_);
            lhs = PyValue(std::floor(lhs.to_float() / r));
        } else {
            throw RuntimeError("TypeError: unsupported operand types for //", pc_);
        }
        NEXT();
    }

    CASE(BINARY_MOD): {
        PyValue rhs = std::move(stack_.back()); stack_.pop_back();
        PyValue& lhs = stack_.back();
        if ((lhs.type() == PyValue::Type::INT || lhs.type() == PyValue::Type::BOOL) &&
            (rhs.type() == PyValue::Type::INT || rhs.type() == PyValue::Type::BOOL)) {
            int64_t r = rhs.to_int();
            if (r == 0) throw RuntimeError("ZeroDivisionError: modulo by zero", pc_);
            lhs = PyValue(lhs.to_int() % r);
        } else if (lhs.is_numeric() && rhs.is_numeric()) {
            double r = rhs.to_float();
            if (r == 0.0) throw RuntimeError("ZeroDivisionError: modulo by zero", pc_);
            lhs = PyValue(std::fmod(lhs.to_float(), r));
        } else {
            throw RuntimeError("TypeError: unsupported operand types for %", pc_);
        }
        NEXT();
    }

    CASE(UNARY_NEG): {
        PyValue& val = stack_.back();
        if (val.type() == PyValue::Type::INT) val = PyValue(-val.as_int());
        else if (val.type() == PyValue::Type::FLOAT) val = PyValue(-val.as_float());
        else throw RuntimeError("TypeError: bad operand type for unary -", pc_);
        NEXT();
    }

    CASE(UNARY_NOT): {
        stack_.back() = PyValue(!stack_.back().truthy());
        NEXT();
    }

    CASE(COMPARE_LT): {
        PyValue rhs = std::move(stack_.back()); stack_.pop_back();
        PyValue& lhs = stack_.back();
        if ((lhs.type() == PyValue::Type::INT || lhs.type() == PyValue::Type::BOOL) &&
            (rhs.type() == PyValue::Type::INT || rhs.type() == PyValue::Type::BOOL)) {
            lhs = PyValue(lhs.to_int() < rhs.to_int());
        } else if (lhs.is_numeric() && rhs.is_numeric()) {
            lhs = PyValue(lhs.to_float() < rhs.to_float());
        } else if (lhs.type() == PyValue::Type::STRING && rhs.type() == PyValue::Type::STRING) {
            lhs = PyValue(lhs.as_string() < rhs.as_string());
        } else {
            throw RuntimeError("TypeError: unsupported comparison", pc_);
        }
        NEXT();
    }

    CASE(COMPARE_GT): {
        PyValue rhs = std::move(stack_.back()); stack_.pop_back();
        PyValue& lhs = stack_.back();
        if ((lhs.type() == PyValue::Type::INT || lhs.type() == PyValue::Type::BOOL) &&
            (rhs.type() == PyValue::Type::INT || rhs.type() == PyValue::Type::BOOL)) {
            lhs = PyValue(lhs.to_int() > rhs.to_int());
        } else {
            lhs = PyValue(lhs > rhs);
        }
        NEXT();
    }

    CASE(COMPARE_EQ): {
        PyValue rhs = std::move(stack_.back()); stack_.pop_back();
        PyValue& lhs = stack_.back();
        if ((lhs.type() == PyValue::Type::INT || lhs.type() == PyValue::Type::BOOL) &&
            (rhs.type() == PyValue::Type::INT || rhs.type() == PyValue::Type::BOOL)) {
            lhs = PyValue(lhs.to_int() == rhs.to_int());
        } else {
            lhs = PyValue(lhs == rhs);
        }
        NEXT();
    }

    CASE(COMPARE_NEQ): {
        PyValue rhs = std::move(stack_.back()); stack_.pop_back();
        PyValue& lhs = stack_.back();
        lhs = PyValue(lhs != rhs);
        NEXT();
    }

    CASE(COMPARE_LTE): {
        PyValue rhs = std::move(stack_.back()); stack_.pop_back();
        PyValue& lhs = stack_.back();
        if ((lhs.type() == PyValue::Type::INT || lhs.type() == PyValue::Type::BOOL) &&
            (rhs.type() == PyValue::Type::INT || rhs.type() == PyValue::Type::BOOL)) {
            lhs = PyValue(lhs.to_int() <= rhs.to_int());
        } else if (lhs.is_numeric() && rhs.is_numeric()) {
            lhs = PyValue(lhs.to_float() <= rhs.to_float());
        } else {
            lhs = PyValue(lhs <= rhs);
        }
        NEXT();
    }

    CASE(COMPARE_GTE): {
        PyValue rhs = std::move(stack_.back()); stack_.pop_back();
        PyValue& lhs = stack_.back();
        lhs = PyValue(lhs >= rhs);
        NEXT();
    }

    CASE(JUMP_IF_FALSE): {
        if (!stack_.back().truthy()) pc_ = instr.operand;
        NEXT();
    }

    CASE(JUMP_IF_TRUE): {
        if (stack_.back().truthy()) pc_ = instr.operand;
        NEXT();
    }

    CASE(JUMP_ABSOLUTE): {
        pc_ = instr.operand;
        NEXT();
    }

    CASE(CALL_FUNCTION): {
        const uint32_t num_args = instr.operand;

        PyValue func_val = std::move(stack_.back()); stack_.pop_back();
        if (func_val.type() != PyValue::Type::FUNCTION) {
            throw RuntimeError("TypeError: object is not callable", pc_);
        }

        auto func = func_val.as_function();

        // Fast path: use cached native function if available
        if (func->native_func && num_args == 1) {
            PyValue arg = std::move(stack_.back()); stack_.pop_back();
            if (arg.type() == PyValue::Type::INT) {
                int64_t result = func->native_func(arg.as_int());
                stack_.push_back(PyValue(result));
                NEXT();
            }
            stack_.push_back(std::move(arg));
        }

        // JIT: check if we should compile
        func->call_count++;
        if (!func->native_func && func->call_count == 1) {
            try {
                auto native = jit_.compile(*code_, func->entry_point, func->params);
                if (native) {
                    func->native_func = native;
                }
            } catch (...) {
                // JIT failed
            }
        }

        // Interpreter path
        const uint32_t num_slots = static_cast<uint32_t>(func->local_slot_names.size());

        CallFrame new_frame;
        new_frame.return_address = pc_;

        if (num_slots > 0) {
            new_frame.init_fast_locals(num_slots);
            const uint32_t base = static_cast<uint32_t>(stack_.size()) - num_args;
            const uint32_t limit = num_args < num_slots ? num_args : num_slots;
            for (uint32_t i = 0; i < limit; ++i) {
                new_frame.get_fast_local(i) = std::move(stack_[base + i]);
            }
            stack_.resize(base);
        }

        frames_.push_back(std::move(new_frame));
        frame = &frames_.back();
        pc_ = func->entry_point;
        NEXT();
    }

    CASE(RETURN_VALUE): {
        PyValue result = std::move(stack_.back()); stack_.pop_back();
        if (frames_.size() > 1) {
            uint32_t ret_addr = frames_.back().return_address;
            frames_.pop_back();
            frame = &frames_.back();
            pc_ = ret_addr;
        }
        stack_.push_back(std::move(result));
        NEXT();
    }

    CASE(PRINT): {
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
        NEXT();
    }

    CASE(MAKE_FUNCTION): { NEXT(); }

    CASE(IMPORT_NAME): {
        const std::string& mod_name = names[instr.operand];

        if (imported_modules_.count(mod_name)) {
            for (auto& [k, v] : imported_modules_[mod_name]) {
                uint32_t idx = code_->find_or_add_name(k);
                if (idx >= globals_.size()) globals_.resize(idx + 1);
                globals_[idx] = v;
            }
            names = code_->names.data();
            num_names = static_cast<uint32_t>(code_->names.size());
            NEXT();
        }

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

        mimo::Lexer mod_lexer(source);
        auto mod_tokens = mod_lexer.tokenize();
        mimo::Parser mod_parser(std::move(mod_tokens));
        auto mod_ast = mod_parser.parse();
        mimo::Compiler mod_compiler;
        auto mod_code = mod_compiler.compile(mod_ast);

        uint32_t instr_offset = num_instrs;
        uint32_t const_offset = static_cast<uint32_t>(code_->constants.size());

        std::vector<uint32_t> name_map(mod_code.names.size());
        for (size_t i = 0; i < mod_code.names.size(); ++i) {
            name_map[i] = code_->find_or_add_name(mod_code.names[i]);
        }

        for (auto& c : mod_code.constants) {
            if (c.type() == PyValue::Type::FUNCTION) {
                auto fn = c.as_function();
                fn->entry_point += instr_offset;
            }
            code_->constants.push_back(std::move(c));
        }

        for (auto& mod_instr : mod_code.instructions) {
            Instruction adjusted = mod_instr;
            switch (mod_instr.op) {
                case OpCode::LOAD_CONST:
                case OpCode::IMPORT_NAME:
                    adjusted.operand += const_offset; break;
                case OpCode::LOAD_NAME:
                case OpCode::STORE_NAME:
                case OpCode::MAKE_FUNCTION:
                    adjusted.operand = name_map[adjusted.operand]; break;
                case OpCode::JUMP_IF_FALSE:
                case OpCode::JUMP_IF_TRUE:
                case OpCode::JUMP_ABSOLUTE:
                    adjusted.operand += instr_offset; break;
                default: break;
            }
            code_->instructions.push_back(adjusted);
        }

        uint32_t return_addr = pc_;
        code_->instructions[instr_offset + mod_code.instructions.size() - 1] =
            Instruction(OpCode::JUMP_ABSOLUTE, return_addr);

        instrs = code_->instructions.data();
        consts = code_->constants.data();
        names = code_->names.data();
        num_instrs = static_cast<uint32_t>(code_->instructions.size());
        num_names = static_cast<uint32_t>(code_->names.size());

        pc_ = instr_offset;
        NEXT();
    }

    CASE(FOR_ITER): { NEXT(); }
    CASE(GET_ITER): { NEXT(); }
    CASE(SETUP_LOOP): { NEXT(); }
    CASE(POP_BLOCK): { NEXT(); }
    CASE(BREAK_LOOP): { NEXT(); }
    CASE(CONTINUE_LOOP): { NEXT(); }
    CASE(NOP): { NEXT(); }

    CASE(HALT): { return; }

#pragma GCC diagnostic pop
#endif
}

} // namespace mimo
