/**
 * @file bytecode.h
 * @brief Bytecode instruction set and instruction representation.
 *
 * Linear bytecode format: each instruction is an opcode + optional operand.
 * The operand is a uint32_t index into the constant pool or a jump target.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <iostream>

namespace mimo {

enum class OpCode : uint8_t {
    LOAD_CONST,       // operand: index into constants pool
    LOAD_NAME,        // operand: index into names pool
    STORE_NAME,       // operand: index into names pool
    POP_TOP,          // pop top of stack

    BINARY_ADD,
    BINARY_SUB,
    BINARY_MUL,
    BINARY_FLOOR_DIV,
    BINARY_MOD,

    UNARY_NEG,
    UNARY_NOT,

    COMPARE_EQ,
    COMPARE_NEQ,
    COMPARE_LT,
    COMPARE_GT,
    COMPARE_LTE,
    COMPARE_GTE,

    JUMP_IF_FALSE,    // operand: absolute address
    JUMP_IF_TRUE,     // operand: absolute address
    JUMP_ABSOLUTE,    // operand: absolute address

    CALL_FUNCTION,    // operand: number of arguments
    RETURN_VALUE,

    PRINT,            // operand: number of arguments
    MAKE_FUNCTION,    // operand: index into names pool (function name)

    FOR_ITER,         // operand: jump past end of loop
    GET_ITER,         // convert top of stack to iterator
    SETUP_LOOP,       // operand: loop end address (for break)
    POP_BLOCK,        // pop loop context

    BREAK_LOOP,       // operand: jump to loop end
    CONTINUE_LOOP,    // operand: jump to loop condition

    DUP_TOP,          // duplicate top of stack
    NOP,              // no operation

    HALT              // stop execution
};

struct Instruction {
    OpCode op;
    uint32_t operand = 0;

    Instruction() : op(OpCode::HALT), operand(0) {}
    Instruction(OpCode o, uint32_t arg = 0) : op(o), operand(arg) {}
};

inline const char* opcode_name(OpCode op) {
    switch (op) {
        case OpCode::LOAD_CONST:       return "LOAD_CONST";
        case OpCode::LOAD_NAME:        return "LOAD_NAME";
        case OpCode::STORE_NAME:       return "STORE_NAME";
        case OpCode::POP_TOP:          return "POP_TOP";
        case OpCode::BINARY_ADD:       return "BINARY_ADD";
        case OpCode::BINARY_SUB:       return "BINARY_SUB";
        case OpCode::BINARY_MUL:       return "BINARY_MUL";
        case OpCode::BINARY_FLOOR_DIV: return "BINARY_FLOOR_DIV";
        case OpCode::BINARY_MOD:       return "BINARY_MOD";
        case OpCode::UNARY_NEG:        return "UNARY_NEG";
        case OpCode::UNARY_NOT:        return "UNARY_NOT";
        case OpCode::COMPARE_EQ:       return "COMPARE_EQ";
        case OpCode::COMPARE_NEQ:      return "COMPARE_NEQ";
        case OpCode::COMPARE_LT:       return "COMPARE_LT";
        case OpCode::COMPARE_GT:       return "COMPARE_GT";
        case OpCode::COMPARE_LTE:      return "COMPARE_LTE";
        case OpCode::COMPARE_GTE:      return "COMPARE_GTE";
        case OpCode::JUMP_IF_FALSE:    return "JUMP_IF_FALSE";
        case OpCode::JUMP_IF_TRUE:     return "JUMP_IF_TRUE";
        case OpCode::JUMP_ABSOLUTE:    return "JUMP_ABSOLUTE";
        case OpCode::CALL_FUNCTION:    return "CALL_FUNCTION";
        case OpCode::RETURN_VALUE:     return "RETURN_VALUE";
        case OpCode::PRINT:            return "PRINT";
        case OpCode::MAKE_FUNCTION:    return "MAKE_FUNCTION";
        case OpCode::FOR_ITER:         return "FOR_ITER";
        case OpCode::GET_ITER:         return "GET_ITER";
        case OpCode::SETUP_LOOP:       return "SETUP_LOOP";
        case OpCode::POP_BLOCK:        return "POP_BLOCK";
        case OpCode::BREAK_LOOP:       return "BREAK_LOOP";
        case OpCode::CONTINUE_LOOP:    return "CONTINUE_LOOP";
        case OpCode::DUP_TOP:          return "DUP_TOP";
        case OpCode::NOP:              return "NOP";
        case OpCode::HALT:             return "HALT";
    }
    return "UNKNOWN";
}

} // namespace mimo
