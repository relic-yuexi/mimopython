/**
 * @file test_vm.cpp
 * @brief Unit tests for the VM.
 */
#include <gtest/gtest.h>
#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "vm.h"
#include <sstream>

using namespace mimo;

class VmTest : public ::testing::Test {
protected:
    std::string run(const std::string& src) {
        Lexer lexer(src);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto ast = parser.parse();
        Compiler compiler;
        auto code = compiler.compile(ast);

        std::ostringstream output;
        Vm vm;
        vm.set_output_stream(output);
        vm.execute(code);
        return output.str();
    }

    std::vector<std::string> run_lines(const std::string& src) {
        Lexer lexer(src);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto ast = parser.parse();
        Compiler compiler;
        auto code = compiler.compile(ast);

        std::ostringstream output;
        Vm vm;
        vm.set_output_stream(output);
        vm.execute(code);

        std::vector<std::string> lines;
        std::string line;
        std::istringstream iss(output.str());
        while (std::getline(iss, line)) {
            lines.push_back(line);
        }
        return lines;
    }
};

TEST_F(VmTest, PrintLiteral) {
    EXPECT_EQ(run("print(42)\n"), "42\n");
}

TEST_F(VmTest, PrintString) {
    EXPECT_EQ(run("print(\"hello\")\n"), "hello\n");
}

TEST_F(VmTest, SimpleArithmetic) {
    EXPECT_EQ(run("print(1 + 2)\n"), "3\n");
}

TEST_F(VmTest, ArithmeticPrecedence) {
    EXPECT_EQ(run("print(2 + 3 * 4)\n"), "14\n");
}

TEST_F(VmTest, ParenthesizedExpression) {
    EXPECT_EQ(run("print((2 + 3) * 4)\n"), "20\n");
}

TEST_F(VmTest, Subtraction) {
    EXPECT_EQ(run("print(10 - 3)\n"), "7\n");
}

TEST_F(VmTest, FloorDivision) {
    EXPECT_EQ(run("print(10 // 3)\n"), "3\n");
}

TEST_F(VmTest, Modulo) {
    EXPECT_EQ(run("print(10 % 3)\n"), "1\n");
}

TEST_F(VmTest, UnaryNeg) {
    EXPECT_EQ(run("print(-42)\n"), "-42\n");
}

TEST_F(VmTest, Comparison) {
    EXPECT_EQ(run("print(1 < 2)\n"), "True\n");
    EXPECT_EQ(run("print(1 > 2)\n"), "False\n");
    EXPECT_EQ(run("print(1 == 1)\n"), "True\n");
    EXPECT_EQ(run("print(1 != 2)\n"), "True\n");
    EXPECT_EQ(run("print(1 <= 1)\n"), "True\n");
    EXPECT_EQ(run("print(1 >= 2)\n"), "False\n");
}

TEST_F(VmTest, LogicalOperators) {
    EXPECT_EQ(run("print(True and True)\n"), "True\n");
    EXPECT_EQ(run("print(True and False)\n"), "False\n");
    EXPECT_EQ(run("print(False or True)\n"), "True\n");
    EXPECT_EQ(run("print(not True)\n"), "False\n");
    EXPECT_EQ(run("print(not False)\n"), "True\n");
}

TEST_F(VmTest, AssignmentAndPrint) {
    EXPECT_EQ(run("x = 42\nprint(x)\n"), "42\n");
}

TEST_F(VmTest, MultipleAssignments) {
    auto lines = run_lines("x = 1\ny = 2\nprint(x + y)\n");
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0], "3");
}

TEST_F(VmTest, IfStatement) {
    EXPECT_EQ(run("x = 1\nif x > 0:\n    print(\"positive\")\n"), "positive\n");
}

TEST_F(VmTest, IfElse) {
    auto lines = run_lines("x = -1\nif x > 0:\n    print(\"positive\")\nelse:\n    print(\"non-positive\")\n");
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0], "non-positive");
}

TEST_F(VmTest, IfElifElse) {
    auto lines = run_lines("x = 0\nif x > 0:\n    print(\"positive\")\nelif x < 0:\n    print(\"negative\")\nelse:\n    print(\"zero\")\n");
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0], "zero");
}

TEST_F(VmTest, WhileLoop) {
    auto lines = run_lines("x = 3\nwhile x > 0:\n    print(x)\n    x = x - 1\n");
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], "3");
    EXPECT_EQ(lines[1], "2");
    EXPECT_EQ(lines[2], "1");
}

TEST_F(VmTest, ForLoop) {
    auto lines = run_lines("for i in range(3):\n    print(i)\n");
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], "0");
    EXPECT_EQ(lines[1], "1");
    EXPECT_EQ(lines[2], "2");
}

TEST_F(VmTest, FunctionDefAndCall) {
    auto lines = run_lines("def add(a, b):\n    return a + b\nprint(add(3, 4))\n");
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0], "7");
}

TEST_F(VmTest, FunctionRecursive) {
    auto lines = run_lines("def fact(n):\n    if n <= 1:\n        return 1\n    return n * fact(n - 1)\nprint(fact(5))\n");
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0], "120");
}

TEST_F(VmTest, FunctionNoReturn) {
    auto lines = run_lines("def noop():\n    pass\nresult = noop()\nprint(result)\n");
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0], "None");
}

TEST_F(VmTest, NestedCalls) {
    auto lines = run_lines("def double(x):\n    return x * 2\ndef add_one(x):\n    return x + 1\nprint(double(add_one(5)))\n");
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0], "12");
}

TEST_F(VmTest, StringConcatenation) {
    EXPECT_EQ(run("print(\"hello\" + \" \" + \"world\")\n"), "hello world\n");
}

TEST_F(VmTest, StringMultiply) {
    EXPECT_EQ(run("print(\"ab\" * 3)\n"), "ababab\n");
}

TEST_F(VmTest, BoolArithmetic) {
    // True is 1, False is 0
    EXPECT_EQ(run("print(True + True)\n"), "2\n");
}

TEST_F(VmTest, FloatArithmetic) {
    EXPECT_EQ(run("print(1.5 + 2.5)\n"), "4.0\n");
}

TEST_F(VmTest, IntFloatMix) {
    EXPECT_EQ(run("print(1 + 2.5)\n"), "3.5\n");
}

TEST_F(VmTest, WhileWithBreak) {
    auto lines = run_lines("x = 0\nwhile True:\n    x = x + 1\n    if x >= 3:\n        break\nprint(x)\n");
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0], "3");
}

TEST_F(VmTest, WhileWithContinue) {
    auto lines = run_lines("x = 0\nwhile x < 5:\n    x = x + 1\n    if x == 3:\n        continue\n    print(x)\n");
    ASSERT_EQ(lines.size(), 4u);
    EXPECT_EQ(lines[0], "1");
    EXPECT_EQ(lines[1], "2");
    EXPECT_EQ(lines[2], "4");
    EXPECT_EQ(lines[3], "5");
}

TEST_F(VmTest, ForLoopWithRangeStartEnd) {
    auto lines = run_lines("for i in range(2, 5):\n    print(i)\n");
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], "2");
    EXPECT_EQ(lines[1], "3");
    EXPECT_EQ(lines[2], "4");
}

TEST_F(VmTest, NestedLoops) {
    auto lines = run_lines("for i in range(3):\n    for j in range(2):\n        print(i * 2 + j)\n");
    ASSERT_EQ(lines.size(), 6u);
    EXPECT_EQ(lines[0], "0");
    EXPECT_EQ(lines[1], "1");
    EXPECT_EQ(lines[2], "2");
    EXPECT_EQ(lines[3], "3");
    EXPECT_EQ(lines[4], "4");
    EXPECT_EQ(lines[5], "5");
}

TEST_F(VmTest, MultiplePrintArgs) {
    auto lines = run_lines("print(1, 2, 3)\n");
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0], "1 2 3");
}

TEST_F(VmTest, NoneComparison) {
    EXPECT_EQ(run("print(None == None)\n"), "True\n");
    EXPECT_EQ(run("print(None != None)\n"), "False\n");
}

TEST_F(VmTest, StringComparison) {
    EXPECT_EQ(run("print(\"abc\" == \"abc\")\n"), "True\n");
    EXPECT_EQ(run("print(\"abc\" < \"abd\")\n"), "True\n");
}
