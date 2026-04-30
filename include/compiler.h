/**
 * @file compiler.h
 * @brief AST -> Bytecode compiler.
 *
 * Walks the AST and emits linear bytecode. Manages the constant pool,
 * name pool, and handles jump backpatching for control flow.
 */
#pragma once

#include "ast.h"
#include "bytecode.h"
#include "value.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>

namespace mimo {

struct CompiledCode {
    std::vector<Instruction> instructions;
    std::vector<PyValue> constants;
    std::vector<std::string> names;

    // Add a constant and return its index
    uint32_t add_constant(PyValue val);
    // Add a name and return its index
    uint32_t add_name(const std::string& name);
    // Find existing name or add new one
    uint32_t find_or_add_name(const std::string& name);
    // Emit an instruction and return its address
    uint32_t emit(Instruction instr);
    // Get current instruction address
    uint32_t current_address() const;
};

class Compiler : public AstVisitor {
public:
    Compiler();
    CompiledCode compile(ProgramNode& program);

private:
    CompiledCode code_;

    // Function-local variable tracking
    bool in_function_ = false;
    std::vector<std::string> local_slots_;  // slot index → name
    std::unordered_map<std::string, uint32_t> local_slot_map_;  // name → slot index

    uint32_t get_or_create_local_slot(const std::string& name);

    // Loop tracking for break/continue
    struct LoopContext {
        uint32_t break_target;    // where break should jump
        uint32_t continue_target; // where continue should jump
        std::vector<uint32_t> break_patches;    // addresses to patch
        std::vector<uint32_t> continue_patches; // addresses to patch
    };
    std::vector<LoopContext> loop_stack_;

    // Visitor implementations
    void visit(IntLiteral& n) override;
    void visit(FloatLiteral& n) override;
    void visit(StringLiteral& n) override;
    void visit(BoolLiteral& n) override;
    void visit(NoneLiteral& n) override;
    void visit(Identifier& n) override;
    void visit(BinaryExpr& n) override;
    void visit(UnaryExpr& n) override;
    void visit(CallExpr& n) override;
    void visit(ExprStmt& n) override;
    void visit(AssignStmt& n) override;
    void visit(PrintStmt& n) override;
    void visit(ReturnStmt& n) override;
    void visit(IfStmt& n) override;
    void visit(WhileStmt& n) override;
    void visit(ForStmt& n) override;
    void visit(FuncDef& n) override;
    void visit(BreakStmt& n) override;
    void visit(ContinueStmt& n) override;
    void visit(PassStmt& n) override;

    void compile_statements(const std::vector<StmtPtr>& stmts);
};

} // namespace mimo
