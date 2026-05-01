/**
 * @file jit.h
 * @brief JIT compiler: compiles hot bytecode functions to native x86-64 code.
 *
 * Uses copy-and-patch approach:
 * 1. Detect hot functions (called N+ times)
 * 2. Compile bytecode to x86-64 machine code
 * 3. Replace function entry with jump to native code
 * 4. Future calls execute natively (no interpreter overhead)
 */
#pragma once

#include "bytecode.h"
#include "value.h"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <memory>

namespace mimo {

struct CompiledCode;
class Vm;

/**
 * @brief Executable memory block with read/write/execute permissions.
 */
class ExecutableMemory {
public:
    ExecutableMemory(size_t size);
    ~ExecutableMemory();

    ExecutableMemory(const ExecutableMemory&) = delete;
    ExecutableMemory& operator=(const ExecutableMemory&) = delete;

    uint8_t* data() { return data_; }
    const uint8_t* data() const { return data_; }
    size_t size() const { return size_; }

    // Make memory executable (after writing code)
    void make_executable();

private:
    uint8_t* data_ = nullptr;
    size_t size_ = 0;
};

/**
 * @brief x86-64 machine code emitter.
 */
class X86Emitter {
public:
    X86Emitter(std::vector<uint8_t>& code) : code_(code) {}

    // Register encoding (x86-64)
    enum Reg : uint8_t {
        RAX = 0, RCX = 1, RDX = 2, RBX = 3,
        RSP = 4, RBP = 5, RSI = 6, RDI = 7,
        R8 = 8, R9 = 9, R10 = 10, R11 = 11,
        R12 = 12, R13 = 13, R14 = 14, R15 = 15
    };

    // mov reg, imm64
    void mov_reg_imm64(Reg dst, int64_t imm);
    // mov reg, reg
    void mov_reg_reg(Reg dst, Reg src);
    // mov reg, [reg + offset]
    void mov_reg_mem(Reg dst, Reg base, int32_t offset);
    // mov [reg + offset], reg
    void mov_mem_reg(Reg base, int32_t offset, Reg src);
    // add reg, reg
    void add_reg_reg(Reg dst, Reg src);
    // sub reg, reg
    void sub_reg_reg(Reg dst, Reg src);
    // imul reg, reg
    void imul_reg_reg(Reg dst, Reg src);
    // cmp reg, reg
    void cmp_reg_reg(Reg a, Reg b);
    // cmp reg, imm32
    void cmp_reg_imm32(Reg a, int32_t imm);
    // jle rel32 (jump if less or equal)
    void jle(int32_t offset);
    // jg rel32 (jump if greater)
    void jg(int32_t offset);
    // jl rel32 (jump if less)
    void jl(int32_t offset);
    // jge rel32 (jump if greater or equal)
    void jge(int32_t offset);
    // je rel32 (jump if equal)
    void je(int32_t offset);
    // jne rel32 (jump if not equal)
    void jne(int32_t offset);
    // jmp rel32
    void jmp(int32_t offset);
    // call reg
    void call_reg(Reg addr);
    // call rel32
    void call_rel32(int32_t offset);
    // ret
    void ret();
    // push reg
    void push_reg(Reg reg);
    // pop reg
    void pop_reg(Reg reg);
    // sub reg, imm32
    void sub_reg_imm32(Reg reg, int32_t imm);
    // add reg, imm32
    void add_reg_imm32(Reg reg, int32_t imm);
    // lea reg, [reg + imm32]
    void lea_reg_reg_imm32(Reg dst, Reg base, int32_t offset);
    // setl reg (set if less)
    void setl(Reg dst);
    // setle reg (set if less or equal)
    void setle(Reg dst);
    // setg reg (set if greater)
    void setg(Reg dst);
    // setge reg (set if greater or equal)
    void setge(Reg dst);
    // sete reg (set if equal)
    void sete(Reg dst);
    // setne reg (set if not equal)
    void setne(Reg dst);
    // movzx reg, reg8 (zero-extend byte to 64-bit)
    void movzx_reg_reg8(Reg dst, Reg src);
    // test reg, reg (logical AND, sets flags)
    void test_reg_reg(Reg a, Reg b);

    // Get current position
    size_t position() const { return code_.size(); }

    // Patch a 32-bit relative offset at the given position
    void patch_rel32(size_t patch_pos, int32_t target_offset);

private:
    std::vector<uint8_t>& code_;

    void emit_u8(uint8_t v);
    void emit_u32(uint32_t v);
    void emit_u64(uint64_t v);
    void emit_rex(Reg reg, Reg rm);
    void emit_modrm(Reg reg, Reg rm);
    void emit_modrm_disp32(Reg reg, Reg base);
};

/**
 * @brief JIT compiler for hot functions.
 */
class JitCompiler {
public:
    JitCompiler();
    ~JitCompiler();

    // Check if a function should be JIT compiled
    bool should_compile(uint32_t func_id) const;

    // Record a function call
    void record_call(uint32_t func_id);

    // Compile a function to native code
    // Returns a function pointer: int64_t(*)(const PyValue* args, uint32_t num_args)
    using NativeFunc = int64_t(*)(int64_t arg);
    NativeFunc compile(const CompiledCode& code, uint32_t entry_point,
                       const std::vector<std::string>& params);

    // Get compiled function if available
    NativeFunc get_compiled(uint32_t func_id) const;

    // Threshold for JIT compilation (set very high to disable)
    static constexpr uint32_t JIT_THRESHOLD = 1000000;

private:
    std::unordered_map<uint32_t, uint32_t> call_counts_;
    std::unordered_map<uint32_t, NativeFunc> compiled_funcs_;
    std::vector<std::unique_ptr<ExecutableMemory>> code_blocks_;

    // Compile bytecode to native code
    void compile_function(const CompiledCode& code, uint32_t entry_point,
                          const std::vector<std::string>& params,
                          std::vector<uint8_t>& native_code,
                          uint8_t* code_addr);
};

} // namespace mimo
