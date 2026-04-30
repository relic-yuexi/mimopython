/**
 * @file ast.h
 * @brief Abstract Syntax Tree node definitions with Visitor pattern.
 *
 * Node hierarchy:
 *   ExprNode  (expressions)
 *   StmtNode  (statements)
 *   ProgramNode (top-level)
 */
#pragma once

#include "token.h"
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace mimo {

// Forward declarations for Visitor
class AstVisitor;

// === Expression Nodes ===

struct ExprNode {
    uint32_t line = 0;
    virtual ~ExprNode() = default;
    virtual void accept(AstVisitor& v) = 0;
};

using ExprPtr = std::unique_ptr<ExprNode>;

struct IntLiteral : ExprNode {
    int64_t value;
    explicit IntLiteral(int64_t val, uint32_t ln) { value = val; line = ln; }
    void accept(AstVisitor& v) override;
};

struct FloatLiteral : ExprNode {
    double value;
    explicit FloatLiteral(double val, uint32_t ln) { value = val; line = ln; }
    void accept(AstVisitor& v) override;
};

struct StringLiteral : ExprNode {
    std::string value;
    explicit StringLiteral(std::string val, uint32_t ln) { value = std::move(val); line = ln; }
    void accept(AstVisitor& v) override;
};

struct BoolLiteral : ExprNode {
    bool value;
    explicit BoolLiteral(bool val, uint32_t ln) { value = val; line = ln; }
    void accept(AstVisitor& v) override;
};

struct NoneLiteral : ExprNode {
    explicit NoneLiteral(uint32_t ln) { line = ln; }
    void accept(AstVisitor& v) override;
};

struct Identifier : ExprNode {
    std::string name;
    explicit Identifier(std::string n, uint32_t ln) { name = std::move(n); line = ln; }
    void accept(AstVisitor& v) override;
};

struct BinaryExpr : ExprNode {
    TokenType op;
    ExprPtr left;
    ExprPtr right;
    BinaryExpr(TokenType o, ExprPtr l, ExprPtr r, uint32_t ln)
        : op(o), left(std::move(l)), right(std::move(r)) { line = ln; }
    void accept(AstVisitor& v) override;
};

struct UnaryExpr : ExprNode {
    TokenType op;
    ExprPtr operand;
    UnaryExpr(TokenType o, ExprPtr expr, uint32_t ln)
        : op(o), operand(std::move(expr)) { line = ln; }
    void accept(AstVisitor& v) override;
};

struct CallExpr : ExprNode {
    std::string callee;
    std::vector<ExprPtr> args;
    CallExpr(std::string c, std::vector<ExprPtr> a, uint32_t ln)
        : callee(std::move(c)), args(std::move(a)) { line = ln; }
    void accept(AstVisitor& v) override;
};

// === Statement Nodes ===

struct StmtNode {
    uint32_t line = 0;
    virtual ~StmtNode() = default;
    virtual void accept(AstVisitor& v) = 0;
};

using StmtPtr = std::unique_ptr<StmtNode>;

struct ExprStmt : StmtNode {
    ExprPtr expr;
    explicit ExprStmt(ExprPtr e, uint32_t ln) : expr(std::move(e)) { line = ln; }
    void accept(AstVisitor& v) override;
};

struct AssignStmt : StmtNode {
    std::string name;
    ExprPtr value;
    AssignStmt(std::string n, ExprPtr val, uint32_t ln)
        : name(std::move(n)), value(std::move(val)) { line = ln; }
    void accept(AstVisitor& v) override;
};

struct PrintStmt : StmtNode {
    std::vector<ExprPtr> args;
    explicit PrintStmt(std::vector<ExprPtr> a, uint32_t ln) : args(std::move(a)) { line = ln; }
    void accept(AstVisitor& v) override;
};

struct ReturnStmt : StmtNode {
    ExprPtr value; // may be null
    explicit ReturnStmt(ExprPtr v, uint32_t ln) : value(std::move(v)) { line = ln; }
    void accept(AstVisitor& v) override;
};

struct IfStmt : StmtNode {
    struct Branch {
        ExprPtr condition; // null for else
        std::vector<StmtPtr> body;
    };
    std::vector<Branch> branches;
    uint32_t line_start;
    explicit IfStmt(uint32_t ln) { line = ln; line_start = ln; }
    void accept(AstVisitor& v) override;
};

struct WhileStmt : StmtNode {
    ExprPtr condition;
    std::vector<StmtPtr> body;
    WhileStmt(ExprPtr cond, std::vector<StmtPtr> b, uint32_t ln)
        : condition(std::move(cond)), body(std::move(b)) { line = ln; }
    void accept(AstVisitor& v) override;
};

struct ForStmt : StmtNode {
    std::string var;
    ExprPtr iterable; // should be range()
    std::vector<StmtPtr> body;
    ForStmt(std::string v, ExprPtr iter, std::vector<StmtPtr> b, uint32_t ln)
        : var(std::move(v)), iterable(std::move(iter)), body(std::move(b)) { line = ln; }
    void accept(AstVisitor& v) override;
};

struct FuncDef : StmtNode {
    std::string name;
    std::vector<std::string> params;
    std::vector<StmtPtr> body;
    FuncDef(std::string n, std::vector<std::string> p, std::vector<StmtPtr> b, uint32_t ln)
        : name(std::move(n)), params(std::move(p)), body(std::move(b)) { line = ln; }
    void accept(AstVisitor& v) override;
};

struct BreakStmt : StmtNode {
    explicit BreakStmt(uint32_t ln) { line = ln; }
    void accept(AstVisitor& v) override;
};

struct ContinueStmt : StmtNode {
    explicit ContinueStmt(uint32_t ln) { line = ln; }
    void accept(AstVisitor& v) override;
};

struct PassStmt : StmtNode {
    explicit PassStmt(uint32_t ln) { line = ln; }
    void accept(AstVisitor& v) override;
};

// === Program ===

struct ProgramNode {
    std::vector<StmtPtr> statements;
};

// === Visitor ===

class AstVisitor {
public:
    virtual ~AstVisitor() = default;
    virtual void visit(IntLiteral& n) = 0;
    virtual void visit(FloatLiteral& n) = 0;
    virtual void visit(StringLiteral& n) = 0;
    virtual void visit(BoolLiteral& n) = 0;
    virtual void visit(NoneLiteral& n) = 0;
    virtual void visit(Identifier& n) = 0;
    virtual void visit(BinaryExpr& n) = 0;
    virtual void visit(UnaryExpr& n) = 0;
    virtual void visit(CallExpr& n) = 0;
    virtual void visit(ExprStmt& n) = 0;
    virtual void visit(AssignStmt& n) = 0;
    virtual void visit(PrintStmt& n) = 0;
    virtual void visit(ReturnStmt& n) = 0;
    virtual void visit(IfStmt& n) = 0;
    virtual void visit(WhileStmt& n) = 0;
    virtual void visit(ForStmt& n) = 0;
    virtual void visit(FuncDef& n) = 0;
    virtual void visit(BreakStmt& n) = 0;
    virtual void visit(ContinueStmt& n) = 0;
    virtual void visit(PassStmt& n) = 0;
};

} // namespace mimo
