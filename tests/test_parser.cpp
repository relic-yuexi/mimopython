/**
 * @file test_parser.cpp
 * @brief Unit tests for the Parser.
 */
#include <gtest/gtest.h>
#include "lexer.h"
#include "parser.h"
#include "ast.h"

using namespace mimo;

class ParserTest : public ::testing::Test {
protected:
    ProgramNode parse(const std::string& src) {
        Lexer lexer(src);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        return parser.parse();
    }
};

TEST_F(ParserTest, SimpleAssignment) {
    auto prog = parse("x = 42\n");
    ASSERT_EQ(prog.statements.size(), 1u);
    auto* assign = dynamic_cast<AssignStmt*>(prog.statements[0].get());
    ASSERT_NE(assign, nullptr);
    EXPECT_EQ(assign->name, "x");
    auto* lit = dynamic_cast<IntLiteral*>(assign->value.get());
    ASSERT_NE(lit, nullptr);
    EXPECT_EQ(lit->value, 42);
}

TEST_F(ParserTest, PrintStatement) {
    auto prog = parse("print(42)\n");
    ASSERT_EQ(prog.statements.size(), 1u);
    auto* print = dynamic_cast<PrintStmt*>(prog.statements[0].get());
    ASSERT_NE(print, nullptr);
    EXPECT_EQ(print->args.size(), 1u);
}

TEST_F(ParserTest, IfStatement) {
    auto prog = parse("if x > 0:\n    y = 1\n");
    ASSERT_EQ(prog.statements.size(), 1u);
    auto* ifstmt = dynamic_cast<IfStmt*>(prog.statements[0].get());
    ASSERT_NE(ifstmt, nullptr);
    EXPECT_EQ(ifstmt->branches.size(), 1u);
    EXPECT_NE(ifstmt->branches[0].condition, nullptr);
    EXPECT_EQ(ifstmt->branches[0].body.size(), 1u);
}

TEST_F(ParserTest, IfElifElse) {
    auto prog = parse("if x > 0:\n    y = 1\nelif x < 0:\n    y = -1\nelse:\n    y = 0\n");
    ASSERT_EQ(prog.statements.size(), 1u);
    auto* ifstmt = dynamic_cast<IfStmt*>(prog.statements[0].get());
    ASSERT_NE(ifstmt, nullptr);
    EXPECT_EQ(ifstmt->branches.size(), 3u);
    EXPECT_NE(ifstmt->branches[0].condition, nullptr);  // if
    EXPECT_NE(ifstmt->branches[1].condition, nullptr);  // elif
    EXPECT_EQ(ifstmt->branches[2].condition, nullptr);   // else
}

TEST_F(ParserTest, WhileStatement) {
    auto prog = parse("while x > 0:\n    x = x - 1\n");
    ASSERT_EQ(prog.statements.size(), 1u);
    auto* wh = dynamic_cast<WhileStmt*>(prog.statements[0].get());
    ASSERT_NE(wh, nullptr);
    EXPECT_NE(wh->condition, nullptr);
    EXPECT_EQ(wh->body.size(), 1u);
}

TEST_F(ParserTest, ForStatement) {
    auto prog = parse("for i in range(10):\n    print(i)\n");
    ASSERT_EQ(prog.statements.size(), 1u);
    auto* forstmt = dynamic_cast<ForStmt*>(prog.statements[0].get());
    ASSERT_NE(forstmt, nullptr);
    EXPECT_EQ(forstmt->var, "i");
    EXPECT_EQ(forstmt->body.size(), 1u);
}

TEST_F(ParserTest, FunctionDef) {
    auto prog = parse("def add(a, b):\n    return a + b\n");
    ASSERT_EQ(prog.statements.size(), 1u);
    auto* func = dynamic_cast<FuncDef*>(prog.statements[0].get());
    ASSERT_NE(func, nullptr);
    EXPECT_EQ(func->name, "add");
    EXPECT_EQ(func->params.size(), 2u);
    EXPECT_EQ(func->params[0], "a");
    EXPECT_EQ(func->params[1], "b");
    EXPECT_EQ(func->body.size(), 1u);
}

TEST_F(ParserTest, FunctionCall) {
    auto prog = parse("print(add(1, 2))\n");
    ASSERT_EQ(prog.statements.size(), 1u);
    auto* printstmt = dynamic_cast<PrintStmt*>(prog.statements[0].get());
    ASSERT_NE(printstmt, nullptr);
    auto* call = dynamic_cast<CallExpr*>(printstmt->args[0].get());
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->callee, "add");
    EXPECT_EQ(call->args.size(), 2u);
}

TEST_F(ParserTest, BinaryExpression) {
    auto prog = parse("x = 1 + 2 * 3\n");
    auto* assign = dynamic_cast<AssignStmt*>(prog.statements[0].get());
    auto* bin = dynamic_cast<BinaryExpr*>(assign->value.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->op, TokenType::PLUS);
    // Right side should be 2 * 3
    auto* right = dynamic_cast<BinaryExpr*>(bin->right.get());
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(right->op, TokenType::STAR);
}

TEST_F(ParserTest, ComparisonOperators) {
    auto prog = parse("x = 1 < 2\n");
    auto* assign = dynamic_cast<AssignStmt*>(prog.statements[0].get());
    auto* bin = dynamic_cast<BinaryExpr*>(assign->value.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->op, TokenType::LT);
}

TEST_F(ParserTest, LogicalOperators) {
    auto prog = parse("x = True and False or True\n");
    auto* assign = dynamic_cast<AssignStmt*>(prog.statements[0].get());
    auto* bin = dynamic_cast<BinaryExpr*>(assign->value.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->op, TokenType::OR);
}

TEST_F(ParserTest, UnaryMinus) {
    auto prog = parse("x = -42\n");
    auto* assign = dynamic_cast<AssignStmt*>(prog.statements[0].get());
    auto* unary = dynamic_cast<UnaryExpr*>(assign->value.get());
    ASSERT_NE(unary, nullptr);
    EXPECT_EQ(unary->op, TokenType::MINUS);
}

TEST_F(ParserTest, NotOperator) {
    auto prog = parse("x = not True\n");
    auto* assign = dynamic_cast<AssignStmt*>(prog.statements[0].get());
    auto* unary = dynamic_cast<UnaryExpr*>(assign->value.get());
    ASSERT_NE(unary, nullptr);
    EXPECT_EQ(unary->op, TokenType::NOT);
}

TEST_F(ParserTest, ParenthesizedExpression) {
    auto prog = parse("x = (1 + 2) * 3\n");
    auto* assign = dynamic_cast<AssignStmt*>(prog.statements[0].get());
    auto* bin = dynamic_cast<BinaryExpr*>(assign->value.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->op, TokenType::STAR);
    auto* left = dynamic_cast<BinaryExpr*>(bin->left.get());
    ASSERT_NE(left, nullptr);
    EXPECT_EQ(left->op, TokenType::PLUS);
}

TEST_F(ParserTest, ReturnWithoutValue) {
    auto prog = parse("def f():\n    return\n");
    auto* func = dynamic_cast<FuncDef*>(prog.statements[0].get());
    ASSERT_NE(func, nullptr);
    auto* ret = dynamic_cast<ReturnStmt*>(func->body[0].get());
    ASSERT_NE(ret, nullptr);
    EXPECT_EQ(ret->value, nullptr);
}

TEST_F(ParserTest, BreakContinuePass) {
    auto prog = parse("while True:\n    break\n    continue\n    pass\n");
    auto* wh = dynamic_cast<WhileStmt*>(prog.statements[0].get());
    ASSERT_NE(wh, nullptr);
    EXPECT_EQ(wh->body.size(), 3u);
    EXPECT_NE(dynamic_cast<BreakStmt*>(wh->body[0].get()), nullptr);
    EXPECT_NE(dynamic_cast<ContinueStmt*>(wh->body[1].get()), nullptr);
    EXPECT_NE(dynamic_cast<PassStmt*>(wh->body[2].get()), nullptr);
}

TEST_F(ParserTest, MultipleStatements) {
    auto prog = parse("x = 1\ny = 2\nz = x + y\n");
    EXPECT_EQ(prog.statements.size(), 3u);
}

TEST_F(ParserTest, FloorDivision) {
    auto prog = parse("x = 10 // 3\n");
    auto* assign = dynamic_cast<AssignStmt*>(prog.statements[0].get());
    auto* bin = dynamic_cast<BinaryExpr*>(assign->value.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->op, TokenType::SLASHSLASH);
}

TEST_F(ParserTest, Modulo) {
    auto prog = parse("x = 10 % 3\n");
    auto* assign = dynamic_cast<AssignStmt*>(prog.statements[0].get());
    auto* bin = dynamic_cast<BinaryExpr*>(assign->value.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->op, TokenType::PERCENT);
}

TEST_F(ParserTest, BoolLiterals) {
    auto prog = parse("x = True\ny = False\n");
    auto* x_assign = dynamic_cast<AssignStmt*>(prog.statements[0].get());
    auto* x_val = dynamic_cast<BoolLiteral*>(x_assign->value.get());
    ASSERT_NE(x_val, nullptr);
    EXPECT_TRUE(x_val->value);

    auto* y_assign = dynamic_cast<AssignStmt*>(prog.statements[1].get());
    auto* y_val = dynamic_cast<BoolLiteral*>(y_assign->value.get());
    ASSERT_NE(y_val, nullptr);
    EXPECT_FALSE(y_val->value);
}

TEST_F(ParserTest, NoneLiteral) {
    auto prog = parse("x = None\n");
    auto* assign = dynamic_cast<AssignStmt*>(prog.statements[0].get());
    auto* val = dynamic_cast<NoneLiteral*>(assign->value.get());
    ASSERT_NE(val, nullptr);
}
