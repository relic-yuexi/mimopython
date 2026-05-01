/**
 * @file jit.h
 * @brief JIT compiler: compiles hot functions to native x86-64 code.
 *
 * The JIT function operates directly on a PyValue stack to avoid conversion overhead.
 * Signature: void(*)(PyValue* stack, int num_args)
 * The function pops args, pushes result onto the stack.
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

class ExecutableMemory {
public:
    ExecutableMemory(size_t size);
    ~ExecutableMemory();
    ExecutableMemory(const ExecutableMemory&) = delete;
    ExecutableMemory& operator=(const ExecutableMemory&) = delete;
    uint8_t* data() { return data_; }
    const uint8_t* data() const { return data_; }
    size_t size() const { return size_; }
    void make_executable();
private:
    uint8_t* data_ = nullptr;
    size_t size_ = 0;
};

class X86Emitter {
public:
    X86Emitter(std::vector<uint8_t>& code) : code_(code) {}

    enum Reg : uint8_t {
        RAX = 0, RCX = 1, RDX = 2, RBX = 3,
        RSP = 4, RBP = 5, RSI = 6, RDI = 7,
        R8 = 8, R9 = 9, R10 = 10, R11 = 11,
        R12 = 12, R13 = 13, R14 = 14, R15 = 15
    };

    void mov_reg_imm64(Reg dst, int64_t imm);
    void mov_reg_reg(Reg dst, Reg src);
    void mov_reg_mem(Reg dst, Reg base, int32_t offset);
    void mov_mem_reg(Reg base, int32_t offset, Reg src);
    void add_reg_reg(Reg dst, Reg src);
    void sub_reg_reg(Reg dst, Reg src);
    void imul_reg_reg(Reg dst, Reg src);
    void cmp_reg_reg(Reg a, Reg b);
    void cmp_reg_imm32(Reg a, int32_t imm);
    void jle(int32_t offset);
    void jg(int32_t offset);
    void jl(int32_t offset);
    void jge(int32_t offset);
    void je(int32_t offset);
    void jne(int32_t offset);
    void jmp(int32_t offset);
    void call_reg(Reg addr);
    void ret();
    void push_reg(Reg reg);
    void pop_reg(Reg reg);
    void sub_reg_imm32(Reg reg, int32_t imm);
    void add_reg_imm32(Reg reg, int32_t imm);
    void lea_reg_reg_imm32(Reg dst, Reg base, int32_t offset);
    void test_reg_reg(Reg a, Reg b);
    void setl(Reg dst);
    void setle(Reg dst);
    void setg(Reg dst);
    void setge(Reg dst);
    void sete(Reg dst);
    void setne(Reg dst);
    void movzx_reg_reg8(Reg dst, Reg src);

    size_t position() const { return code_.size(); }
    void patch_rel32(size_t patch_pos, int32_t target_offset);

private:
    std::vector<uint8_t>& code_;
    void emit_u8(uint8_t v);
    void emit_u32(uint32_t v);
    void emit_u64(uint64_t v);
    void emit_rex(Reg reg, Reg rm);
    void emit_modrm(Reg reg, Reg rm);
};

class JitCompiler {
public:
    JitCompiler();
    ~JitCompiler();

    using NativeFunc = int64_t(*)(int64_t);

    bool should_compile(uint32_t func_id) const;
    void record_call(uint32_t func_id);
    NativeFunc get_compiled(uint32_t func_id) const;
    NativeFunc compile(const CompiledCode& code, uint32_t entry_point,
                       const std::vector<std::string>& params);

    static constexpr uint32_t JIT_THRESHOLD = 1;

private:
    std::unordered_map<uint32_t, uint32_t> call_counts_;
    std::unordered_map<uint32_t, NativeFunc> compiled_funcs_;
    std::vector<std::unique_ptr<ExecutableMemory>> code_blocks_;

    void compile_function(const CompiledCode& code, uint32_t entry_point,
                          const std::vector<std::string>& params,
                          std::vector<uint8_t>& native_code,
                          uint8_t* code_addr);
};

} // namespace mimo
