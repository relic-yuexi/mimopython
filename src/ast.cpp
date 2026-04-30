/**
 * @file ast.cpp
 * @brief AST node accept() implementations.
 */
#include "ast.h"

namespace mimo {

void IntLiteral::accept(AstVisitor& v) { v.visit(*this); }
void FloatLiteral::accept(AstVisitor& v) { v.visit(*this); }
void StringLiteral::accept(AstVisitor& v) { v.visit(*this); }
void BoolLiteral::accept(AstVisitor& v) { v.visit(*this); }
void NoneLiteral::accept(AstVisitor& v) { v.visit(*this); }
void Identifier::accept(AstVisitor& v) { v.visit(*this); }
void BinaryExpr::accept(AstVisitor& v) { v.visit(*this); }
void UnaryExpr::accept(AstVisitor& v) { v.visit(*this); }
void CallExpr::accept(AstVisitor& v) { v.visit(*this); }
void ExprStmt::accept(AstVisitor& v) { v.visit(*this); }
void AssignStmt::accept(AstVisitor& v) { v.visit(*this); }
void PrintStmt::accept(AstVisitor& v) { v.visit(*this); }
void ReturnStmt::accept(AstVisitor& v) { v.visit(*this); }
void IfStmt::accept(AstVisitor& v) { v.visit(*this); }
void WhileStmt::accept(AstVisitor& v) { v.visit(*this); }
void ForStmt::accept(AstVisitor& v) { v.visit(*this); }
void FuncDef::accept(AstVisitor& v) { v.visit(*this); }
void BreakStmt::accept(AstVisitor& v) { v.visit(*this); }
void ContinueStmt::accept(AstVisitor& v) { v.visit(*this); }
void PassStmt::accept(AstVisitor& v) { v.visit(*this); }

} // namespace mimo
