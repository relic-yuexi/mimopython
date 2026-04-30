/**
 * @file test_compiler.cpp
 * @brief Unit tests for the Compiler.
 */
#include <gtest/gtest.h>
#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "bytecode.h"

using namespace mimo;

class CompilerTest : public ::testing::Test {
protected:
    CompiledCode compile(const std::string& src) {
        Lexer lexer(src);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto ast = parser.parse();
        Compiler compiler;
        return compiler.compile(ast);
    }
};

TEST_F(CompilerTest, SimpleAssignment) {
    auto code = compile("x = 42\n");
    // Should have: LOAD_CONST, STORE_NAME, HALT
    ASSERT_GE(code.instructions.size(), 3u);
    EXPECT_EQ(code.instructions[0].op, OpCode::LOAD_CONST);
    EXPECT_EQ(code.instructions[1].op, OpCode::STORE_NAME);
    EXPECT_EQ(code.instructions.back().op, OpCode::HALT);
}

TEST_F(CompilerTest, PrintStatement) {
    auto code = compile("print(42)\n");
    // LOAD_CONST, PRINT, HALT
    ASSERT_GE(code.instructions.size(), 3u);
    EXPECT_EQ(code.instructions[0].op, OpCode::LOAD_CONST);
    EXPECT_EQ(code.instructions[1].op, OpCode::PRINT);
    EXPECT_EQ(code.instructions[1].operand, 1u);
}

TEST_F(CompilerTest, BinaryAddition) {
    auto code = compile("x = 1 + 2\n");
    // LOAD_CONST(1), LOAD_CONST(2), BINARY_ADD, STORE_NAME, HALT
    ASSERT_GE(code.instructions.size(), 5u);
    EXPECT_EQ(code.instructions[0].op, OpCode::LOAD_CONST);
    EXPECT_EQ(code.instructions[1].op, OpCode::LOAD_CONST);
    EXPECT_EQ(code.instructions[2].op, OpCode::BINARY_ADD);
    EXPECT_EQ(code.instructions[3].op, OpCode::STORE_NAME);
}

TEST_F(CompilerTest, Comparison) {
    auto code = compile("x = 1 < 2\n");
    // LOAD_CONST, LOAD_CONST, COMPARE_LT, STORE_NAME, HALT
    ASSERT_GE(code.instructions.size(), 5u);
    EXPECT_EQ(code.instructions[2].op, OpCode::COMPARE_LT);
}

TEST_F(CompilerTest, JumpIfFalse) {
    auto code = compile("if True:\n    x = 1\n");
    // LOAD_CONST, JUMP_IF_FALSE, POP_TOP, LOAD_CONST, STORE_NAME, JUMP_ABSOLUTE, POP_TOP, HALT
    bool has_jump = false;
    for (auto& i : code.instructions) {
        if (i.op == OpCode::JUMP_IF_FALSE) has_jump = true;
    }
    EXPECT_TRUE(has_jump);
}

TEST_F(CompilerTest, WhileLoop) {
    auto code = compile("while True:\n    pass\n");
    bool has_jump_back = false;
    for (auto& i : code.instructions) {
        if (i.op == OpCode::JUMP_ABSOLUTE) has_jump_back = true;
    }
    EXPECT_TRUE(has_jump_back);
}

TEST_F(CompilerTest, FunctionDef) {
    auto code = compile("def f(x):\n    return x\n");
    // JUMP_ABSOLUTE (skip), STORE_NAME (param), LOAD_NAME (x), RETURN_VALUE, LOAD_CONST (None), RETURN_VALUE, LOAD_CONST (func), STORE_NAME, HALT
    bool has_func = false;
    for (auto& c : code.constants) {
        if (c.type() == PyValue::Type::FUNCTION) has_func = true;
    }
    EXPECT_TRUE(has_func);
}

TEST_F(CompilerTest, FunctionCall) {
    auto code = compile("def f(x):\n    return x\nf(42)\n");
    // Should have CALL_FUNCTION instruction
    bool has_call = false;
    for (auto& i : code.instructions) {
        if (i.op == OpCode::CALL_FUNCTION) has_call = true;
    }
    EXPECT_TRUE(has_call);
}

TEST_F(CompilerTest, ForLoop) {
    auto code = compile("for i in range(5):\n    print(i)\n");
    // Should have comparison and jump instructions
    bool has_compare = false;
    for (auto& i : code.instructions) {
        if (i.op == OpCode::COMPARE_LT) has_compare = true;
    }
    EXPECT_TRUE(has_compare);
}

TEST_F(CompilerTest, UnaryNeg) {
    auto code = compile("x = -42\n");
    // LOAD_CONST, UNARY_NEG, STORE_NAME, HALT
    ASSERT_GE(code.instructions.size(), 4u);
    EXPECT_EQ(code.instructions[1].op, OpCode::UNARY_NEG);
}

TEST_F(CompilerTest, LogicalAnd) {
    auto code = compile("x = True and False\n");
    // LOAD_CONST(True), JUMP_IF_FALSE, POP_TOP, LOAD_CONST(False), STORE_NAME, HALT
    bool has_jump = false;
    for (auto& i : code.instructions) {
        if (i.op == OpCode::JUMP_IF_FALSE) has_jump = true;
    }
    EXPECT_TRUE(has_jump);
}

TEST_F(CompilerTest, LogicalOr) {
    auto code = compile("x = True or False\n");
    // LOAD_CONST(True), DUP_TOP, JUMP_IF_TRUE, POP_TOP, LOAD_CONST(False), STORE_NAME, HALT
    bool has_jump = false;
    for (auto& i : code.instructions) {
        if (i.op == OpCode::JUMP_IF_TRUE) has_jump = true;
    }
    EXPECT_TRUE(has_jump);
}

TEST_F(CompilerTest, BreakInLoop) {
    auto code = compile("while True:\n    break\n");
    bool has_break_jump = false;
    for (auto& i : code.instructions) {
        if (i.op == OpCode::JUMP_ABSOLUTE && i.operand > 0) {
            // Check if this is a break jump (should jump past the loop)
            has_break_jump = true;
        }
    }
    EXPECT_TRUE(has_break_jump);
}

TEST_F(CompilerTest, ConstantsPool) {
    auto code = compile("x = 42\ny = \"hello\"\n");
    // Should have at least 2 constants
    ASSERT_GE(code.constants.size(), 2u);
}

TEST_F(CompilerTest, NamesPool) {
    auto code = compile("x = 1\ny = 2\n");
    // Should have at least 2 names
    ASSERT_GE(code.names.size(), 2u);
}
