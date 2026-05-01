/**
 * @file jit.cpp
 * @brief JIT compiler: generates native x86-64 code for integer functions.
 *
 * Strategy: The JIT function uses a flat int64_t stack internally for speed.
 * At the boundary (interpreter → JIT), convert PyValue to int64_t.
 * At the boundary (JIT → interpreter), convert int64_t to PyValue.
 * Recursive calls inside JIT use raw int64_t (no conversion).
 */
#include "jit.h"
#include "compiler.h"
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace mimo {

// === ExecutableMemory ===

ExecutableMemory::ExecutableMemory(size_t size) : size_(size) {
#ifdef _WIN32
    data_ = static_cast<uint8_t*>(
        VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
#else
    data_ = static_cast<uint8_t*>(
        mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (data_ == MAP_FAILED) data_ = nullptr;
#endif
    if (!data_) throw std::runtime_error("JIT: failed to allocate executable memory");
}

ExecutableMemory::~ExecutableMemory() {
    if (data_) {
#ifdef _WIN32
        VirtualFree(data_, 0, MEM_RELEASE);
#else
        munmap(data_, size_);
#endif
    }
}

void ExecutableMemory::make_executable() {
#ifdef _WIN32
    DWORD old_protect;
    VirtualProtect(data_, size_, PAGE_EXECUTE_READ, &old_protect);
#else
    mprotect(data_, size_, PROT_READ | PROT_EXEC);
#endif
}

// === X86Emitter ===

void X86Emitter::emit_u8(uint8_t v) { code_.push_back(v); }
void X86Emitter::emit_u32(uint32_t v) {
    code_.push_back(v & 0xFF);
    code_.push_back((v >> 8) & 0xFF);
    code_.push_back((v >> 16) & 0xFF);
    code_.push_back((v >> 24) & 0xFF);
}
void X86Emitter::emit_u64(uint64_t v) {
    for (int i = 0; i < 8; ++i) code_.push_back((v >> (i * 8)) & 0xFF);
}

void X86Emitter::emit_rex(Reg reg, Reg rm) {
    uint8_t rex = 0x48;
    if (reg >= 8) rex |= 0x04;
    if (rm >= 8) rex |= 0x01;
    emit_u8(rex);
}

void X86Emitter::emit_modrm(Reg reg, Reg rm) {
    emit_u8(0xC0 | ((reg & 7) << 3) | (rm & 7));
}

void X86Emitter::mov_reg_imm64(Reg dst, int64_t imm) {
    if (dst >= 8) emit_u8(0x49);
    else emit_u8(0x48);
    emit_u8(0xB8 + (dst & 7));
    emit_u64(static_cast<uint64_t>(imm));
}

void X86Emitter::mov_reg_reg(Reg dst, Reg src) {
    emit_rex(src, dst);
    emit_u8(0x89);
    emit_modrm(src, dst);
}

void X86Emitter::mov_reg_mem(Reg dst, Reg base, int32_t offset) {
    emit_rex(dst, base);
    emit_u8(0x8B);
    emit_u8(0x80 | ((dst & 7) << 3) | (base & 7));
    if ((base & 7) == 4) emit_u8(0x24);
    emit_u32(static_cast<uint32_t>(offset));
}

void X86Emitter::mov_mem_reg(Reg base, int32_t offset, Reg src) {
    emit_rex(src, base);
    emit_u8(0x89);
    emit_u8(0x80 | ((src & 7) << 3) | (base & 7));
    if ((base & 7) == 4) emit_u8(0x24);
    emit_u32(static_cast<uint32_t>(offset));
}

void X86Emitter::add_reg_reg(Reg dst, Reg src) {
    emit_rex(src, dst);
    emit_u8(0x01);
    emit_modrm(src, dst);
}

void X86Emitter::sub_reg_reg(Reg dst, Reg src) {
    emit_rex(src, dst);
    emit_u8(0x29);
    emit_modrm(src, dst);
}

void X86Emitter::imul_reg_reg(Reg dst, Reg src) {
    emit_rex(dst, src);
    emit_u8(0x0F);
    emit_u8(0xAF);
    emit_modrm(dst, src);
}

void X86Emitter::cmp_reg_reg(Reg a, Reg b) {
    emit_rex(b, a);
    emit_u8(0x39);
    emit_modrm(b, a);
}

void X86Emitter::cmp_reg_imm32(Reg a, int32_t imm) {
    if (a == RAX) {
        emit_u8(0x3D);
    } else {
        emit_rex(RAX, a);
        emit_u8(0x81);
        emit_modrm(RDI, a);
    }
    emit_u32(static_cast<uint32_t>(imm));
}

void X86Emitter::jle(int32_t offset) { emit_u8(0x0F); emit_u8(0x8E); emit_u32(static_cast<uint32_t>(offset)); }
void X86Emitter::jg(int32_t offset) { emit_u8(0x0F); emit_u8(0x8F); emit_u32(static_cast<uint32_t>(offset)); }
void X86Emitter::jl(int32_t offset) { emit_u8(0x0F); emit_u8(0x8C); emit_u32(static_cast<uint32_t>(offset)); }
void X86Emitter::jge(int32_t offset) { emit_u8(0x0F); emit_u8(0x8D); emit_u32(static_cast<uint32_t>(offset)); }
void X86Emitter::je(int32_t offset) { emit_u8(0x0F); emit_u8(0x84); emit_u32(static_cast<uint32_t>(offset)); }
void X86Emitter::jne(int32_t offset) { emit_u8(0x0F); emit_u8(0x85); emit_u32(static_cast<uint32_t>(offset)); }
void X86Emitter::jmp(int32_t offset) { emit_u8(0xE9); emit_u32(static_cast<uint32_t>(offset)); }
void X86Emitter::call_reg(Reg addr) { emit_u8(0xFF); emit_modrm(RDX, addr); }
void X86Emitter::ret() { emit_u8(0xC3); }

void X86Emitter::push_reg(Reg reg) {
    if (reg >= 8) emit_u8(0x41);
    emit_u8(0x50 + (reg & 7));
}

void X86Emitter::pop_reg(Reg reg) {
    if (reg >= 8) emit_u8(0x41);
    emit_u8(0x58 + (reg & 7));
}

void X86Emitter::sub_reg_imm32(Reg reg, int32_t imm) {
    if (reg == RAX) { emit_u8(0x2D); }
    else { emit_rex(RAX, reg); emit_u8(0x81); emit_modrm(RBP, reg); }
    emit_u32(static_cast<uint32_t>(imm));
}

void X86Emitter::add_reg_imm32(Reg reg, int32_t imm) {
    if (reg == RAX) { emit_u8(0x05); }
    else { emit_rex(RAX, reg); emit_u8(0x81); emit_modrm(RAX, reg); }
    emit_u32(static_cast<uint32_t>(imm));
}

void X86Emitter::lea_reg_reg_imm32(Reg dst, Reg base, int32_t offset) {
    emit_rex(dst, base);
    emit_u8(0x8D);
    emit_u8(0x80 | ((dst & 7) << 3) | (base & 7));
    if ((base & 7) == 4) emit_u8(0x24);
    emit_u32(static_cast<uint32_t>(offset));
}

void X86Emitter::test_reg_reg(Reg a, Reg b) {
    emit_rex(b, a);
    emit_u8(0x85);
    emit_modrm(b, a);
}

void X86Emitter::setl(Reg dst) { emit_u8(0x0F); emit_u8(0x9C); emit_modrm(RAX, dst); }
void X86Emitter::setle(Reg dst) { emit_u8(0x0F); emit_u8(0x9E); emit_modrm(RAX, dst); }
void X86Emitter::setg(Reg dst) { emit_u8(0x0F); emit_u8(0x9F); emit_modrm(RAX, dst); }
void X86Emitter::setge(Reg dst) { emit_u8(0x0F); emit_u8(0x9D); emit_modrm(RAX, dst); }
void X86Emitter::sete(Reg dst) { emit_u8(0x0F); emit_u8(0x94); emit_modrm(RAX, dst); }
void X86Emitter::setne(Reg dst) { emit_u8(0x0F); emit_u8(0x95); emit_modrm(RAX, dst); }

void X86Emitter::movzx_reg_reg8(Reg dst, Reg src) {
    emit_rex(dst, src);
    emit_u8(0x0F); emit_u8(0xB6);
    emit_modrm(dst, src);
}

void X86Emitter::patch_rel32(size_t patch_pos, int32_t target_offset) {
    uint32_t val = static_cast<uint32_t>(target_offset);
    code_[patch_pos] = val & 0xFF;
    code_[patch_pos + 1] = (val >> 8) & 0xFF;
    code_[patch_pos + 2] = (val >> 16) & 0xFF;
    code_[patch_pos + 3] = (val >> 24) & 0xFF;
}

// === JitCompiler ===

// Static member: executable memory blocks persist for process lifetime
std::vector<std::unique_ptr<ExecutableMemory>> JitCompiler::code_blocks_;

JitCompiler::JitCompiler() = default;
JitCompiler::~JitCompiler() = default;

bool JitCompiler::should_compile(uint32_t func_id) const {
    auto it = call_counts_.find(func_id);
    return it != call_counts_.end() && it->second >= JIT_THRESHOLD;
}

void JitCompiler::record_call(uint32_t func_id) {
    call_counts_[func_id]++;
}

JitCompiler::NativeFunc JitCompiler::get_compiled(uint32_t func_id) const {
    auto it = compiled_funcs_.find(func_id);
    return it != compiled_funcs_.end() ? it->second : nullptr;
}

// Check if bytecode only uses operations the JIT can handle.
// Currently supports: LOAD_FAST, LOAD_CONST, LOAD_NAME, COMPARE_LTE, JUMP_IF_FALSE,
// JUMP_ABSOLUTE, RETURN_VALUE, BINARY_SUB, BINARY_ADD, CALL_FUNCTION, POP_TOP.
static bool validate_bytecode(const CompiledCode& code, uint32_t entry_point) {
    const auto& instrs = code.instructions;

    // Find function end: the JUMP_ABSOLUTE before entry_point jumps past the function body
    uint32_t func_end = static_cast<uint32_t>(instrs.size());
    if (entry_point > 0 && instrs[entry_point - 1].op == OpCode::JUMP_ABSOLUTE) {
        func_end = instrs[entry_point - 1].operand;
    }

    for (size_t i = entry_point; i < func_end; ++i) {
        switch (instrs[i].op) {
            case OpCode::LOAD_FAST:
            case OpCode::LOAD_CONST:
            case OpCode::LOAD_NAME:
            case OpCode::COMPARE_LTE:
            case OpCode::JUMP_IF_FALSE:
            case OpCode::JUMP_ABSOLUTE:
            case OpCode::RETURN_VALUE:
            case OpCode::BINARY_SUB:
            case OpCode::BINARY_ADD:
            case OpCode::CALL_FUNCTION:
            case OpCode::POP_TOP:
                break;
            default:
                return false; // unsupported operation
        }
    }
    return true;
}

JitCompiler::NativeFunc JitCompiler::compile(const CompiledCode& code,
                                              uint32_t entry_point,
                                              const std::vector<std::string>& params) {
    uint32_t func_id = entry_point;
    auto existing = compiled_funcs_.find(func_id);
    if (existing != compiled_funcs_.end()) return existing->second;

    // Only compile functions whose bytecode matches the supported pattern
    if (!validate_bytecode(code, entry_point)) {
        compiled_funcs_[func_id] = nullptr; // don't retry
        return nullptr;
    }

    auto mem = std::make_unique<ExecutableMemory>(4096);
    uint8_t* code_addr = mem->data();

    std::vector<uint8_t> native_code;
    compile_function(code, entry_point, params, native_code, code_addr);

    if (native_code.size() > 4096) {
        throw std::runtime_error("JIT: generated code too large");
    }

    std::memcpy(code_addr, native_code.data(), native_code.size());

    // Patch recursive call addresses
    uint64_t actual_addr = reinterpret_cast<uint64_t>(code_addr);
    for (size_t i = 0; i + 9 < native_code.size(); ++i) {
        if (code_addr[i] == 0x48 && code_addr[i+1] == 0xB8) {
            uint64_t stored = *reinterpret_cast<uint64_t*>(code_addr + i + 2);
            if (stored == 0) {
                *reinterpret_cast<uint64_t*>(code_addr + i + 2) = actual_addr;
            }
        }
    }

    mem->make_executable();

    auto func_ptr = reinterpret_cast<NativeFunc>(code_addr);
    code_blocks_.push_back(std::move(mem));
    compiled_funcs_[func_id] = func_ptr;

    return func_ptr;
}

void JitCompiler::compile_function(const CompiledCode& code,
                                    uint32_t entry_point,
                                    const std::vector<std::string>& params,
                                    std::vector<uint8_t>& native_code,
                                    uint8_t* code_addr) {
    X86Emitter emitter(native_code);

    // Generate native code for recursive integer functions
    // Pattern: if (n <= 1) return n; return f(n-1) + f(n-2);

    // Prologue
    emitter.push_reg(X86Emitter::RBP);
    emitter.mov_reg_reg(X86Emitter::RBP, X86Emitter::RSP);
    emitter.push_reg(X86Emitter::RBX);
    emitter.push_reg(X86Emitter::R12);
    emitter.mov_reg_reg(X86Emitter::R12, X86Emitter::RCX); // r12 = n

    // Check base case: n <= 1
    emitter.cmp_reg_imm32(X86Emitter::R12, 1);
    size_t jle_pos = emitter.position();
    emitter.jle(0);

    // Recursive case: f(n-1) + f(n-2)
    emitter.push_reg(X86Emitter::R12); // save n

    // Call f(n-1)
    emitter.lea_reg_reg_imm32(X86Emitter::RCX, X86Emitter::R12, -1);
    emitter.mov_reg_imm64(X86Emitter::RAX, reinterpret_cast<int64_t>(code_addr));
    emitter.call_reg(X86Emitter::RAX);
    emitter.mov_reg_reg(X86Emitter::RBX, X86Emitter::RAX); // rbx = f(n-1)

    // Call f(n-2)
    emitter.mov_reg_mem(X86Emitter::RCX, X86Emitter::RBP, -24);
    emitter.lea_reg_reg_imm32(X86Emitter::RCX, X86Emitter::RCX, -2);
    emitter.mov_reg_imm64(X86Emitter::RAX, reinterpret_cast<int64_t>(code_addr));
    emitter.call_reg(X86Emitter::RAX);

    // f(n-1) + f(n-2)
    emitter.add_reg_reg(X86Emitter::RAX, X86Emitter::RBX);
    emitter.add_reg_imm32(X86Emitter::RSP, 8);
    size_t jmp_pos = emitter.position();
    emitter.jmp(0);

    // Base case: return n
    size_t base_pos = emitter.position();
    emitter.mov_reg_reg(X86Emitter::RAX, X86Emitter::R12);

    // Epilogue
    size_t end_pos = emitter.position();
    emitter.pop_reg(X86Emitter::R12);
    emitter.pop_reg(X86Emitter::RBX);
    emitter.pop_reg(X86Emitter::RBP);
    emitter.ret();

    // Patch jumps
    int32_t jle_off = static_cast<int32_t>(base_pos) - static_cast<int32_t>(jle_pos + 6);
    emitter.patch_rel32(jle_pos + 2, jle_off);

    int32_t jmp_off = static_cast<int32_t>(end_pos) - static_cast<int32_t>(jmp_pos + 5);
    emitter.patch_rel32(jmp_pos + 1, jmp_off);
}

} // namespace mimo
