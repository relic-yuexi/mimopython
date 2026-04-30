/**
 * @file test_integration.cpp
 * @brief Integration tests: run complete Python programs and verify output.
 */
#include <gtest/gtest.h>
#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "vm.h"
#include <sstream>
#include <string>
#include <vector>

using namespace mimo;

class IntegrationTest : public ::testing::Test {
protected:
    std::vector<std::string> run_program(const std::string& src) {
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

    void expect_output(const std::string& src, const std::vector<std::string>& expected) {
        auto lines = run_program(src);
        ASSERT_EQ(lines.size(), expected.size()) << "Output:\n" << [&]() {
            std::string s;
            for (auto& l : lines) s += l + "\n";
            return s;
        }();
        for (size_t i = 0; i < expected.size(); ++i) {
            EXPECT_EQ(lines[i], expected[i]) << "Line " << i;
        }
    }
};

// Test 1: Basic arithmetic
TEST_F(IntegrationTest, Arithmetic) {
    expect_output(
        "print(1 + 2)\n"
        "print(10 - 3)\n"
        "print(4 * 5)\n"
        "print(10 // 3)\n"
        "print(10 % 3)\n",
        {"3", "7", "20", "3", "1"}
    );
}

// Test 2: Variables and assignment
TEST_F(IntegrationTest, Variables) {
    expect_output(
        "x = 10\n"
        "y = 20\n"
        "z = x + y\n"
        "print(z)\n",
        {"30"}
    );
}

// Test 3: If/elif/else
TEST_F(IntegrationTest, Conditionals) {
    expect_output(
        "x = 5\n"
        "if x > 10:\n"
        "    print(\"big\")\n"
        "elif x > 3:\n"
        "    print(\"medium\")\n"
        "else:\n"
        "    print(\"small\")\n",
        {"medium"}
    );
}

// Test 4: While loop
TEST_F(IntegrationTest, WhileLoop) {
    expect_output(
        "i = 0\n"
        "while i < 5:\n"
        "    print(i)\n"
        "    i = i + 1\n",
        {"0", "1", "2", "3", "4"}
    );
}

// Test 5: For loop with range
TEST_F(IntegrationTest, ForLoop) {
    expect_output(
        "for i in range(5):\n"
        "    print(i)\n",
        {"0", "1", "2", "3", "4"}
    );
}

// Test 6: Function definition and call
TEST_F(IntegrationTest, Functions) {
    expect_output(
        "def add(a, b):\n"
        "    return a + b\n"
        "print(add(3, 4))\n"
        "print(add(10, 20))\n",
        {"7", "30"}
    );
}

// Test 7: Recursive function (factorial)
TEST_F(IntegrationTest, Factorial) {
    expect_output(
        "def fact(n):\n"
        "    if n <= 1:\n"
        "        return 1\n"
        "    return n * fact(n - 1)\n"
        "print(fact(5))\n"
        "print(fact(10))\n",
        {"120", "3628800"}
    );
}

// Test 8: Fibonacci
TEST_F(IntegrationTest, Fibonacci) {
    expect_output(
        "def fib(n):\n"
        "    if n <= 1:\n"
        "        return n\n"
        "    return fib(n - 1) + fib(n - 2)\n"
        "for i in range(7):\n"
        "    print(fib(i))\n",
        {"0", "1", "1", "2", "3", "5", "8"}
    );
}

// Test 9: Nested loops
TEST_F(IntegrationTest, NestedLoops) {
    expect_output(
        "for i in range(3):\n"
        "    for j in range(2):\n"
        "        print(i, j)\n",
        {"0 0", "0 1", "1 0", "1 1", "2 0", "2 1"}
    );
}

// Test 10: Break and continue
TEST_F(IntegrationTest, BreakContinue) {
    expect_output(
        "for i in range(10):\n"
        "    if i == 3:\n"
        "        break\n"
        "    print(i)\n",
        {"0", "1", "2"}
    );
    expect_output(
        "for i in range(5):\n"
        "    if i == 2:\n"
        "        continue\n"
        "    print(i)\n",
        {"0", "1", "3", "4"}
    );
}

// Test 11: String operations
TEST_F(IntegrationTest, Strings) {
    expect_output(
        "print(\"hello\" + \" \" + \"world\")\n"
        "print(\"ab\" * 3)\n",
        {"hello world", "ababab"}
    );
}

// Test 12: Boolean logic
TEST_F(IntegrationTest, BooleanLogic) {
    expect_output(
        "print(True and True)\n"
        "print(True and False)\n"
        "print(False or True)\n"
        "print(not False)\n",
        {"True", "False", "True", "True"}
    );
}

// Test 13: Comparison operators
TEST_F(IntegrationTest, Comparisons) {
    expect_output(
        "print(1 < 2)\n"
        "print(1 > 2)\n"
        "print(1 == 1)\n"
        "print(1 != 2)\n"
        "print(1 <= 1)\n"
        "print(1 >= 2)\n",
        {"True", "False", "True", "True", "True", "False"}
    );
}

// Test 14: Complex expression
TEST_F(IntegrationTest, ComplexExpression) {
    expect_output(
        "x = 2 + 3 * 4\n"
        "print(x)\n"
        "y = (2 + 3) * 4\n"
        "print(y)\n",
        {"14", "20"}
    );
}

// Test 15: Function with multiple parameters
TEST_F(IntegrationTest, MultiParamFunction) {
    expect_output(
        "def sum3(a, b, c):\n"
        "    return a + b + c\n"
        "print(sum3(1, 2, 3))\n",
        {"6"}
    );
}

// Test 16: Nested function calls
TEST_F(IntegrationTest, NestedCalls) {
    expect_output(
        "def double(x):\n"
        "    return x * 2\n"
        "def add_one(x):\n"
        "    return x + 1\n"
        "print(double(add_one(5)))\n",
        {"12"}
    );
}

// Test 17: While with break
TEST_F(IntegrationTest, WhileBreak) {
    expect_output(
        "x = 0\n"
        "while True:\n"
        "    x = x + 1\n"
        "    if x >= 5:\n"
        "        break\n"
        "print(x)\n",
        {"5"}
    );
}

// Test 18: Pass statement
TEST_F(IntegrationTest, PassStatement) {
    expect_output(
        "x = 1\n"
        "if x > 0:\n"
        "    pass\n"
        "print(x)\n",
        {"1"}
    );
}

// Test 19: Float arithmetic
TEST_F(IntegrationTest, FloatArithmetic) {
    expect_output(
        "print(1.5 + 2.5)\n"
        "print(3.0 * 2.0)\n",
        {"4.0", "6.0"}
    );
}

// Test 20: Mixed int/float
TEST_F(IntegrationTest, MixedNumeric) {
    expect_output(
        "print(1 + 2.5)\n"
        "print(3.0 * 2)\n",
        {"3.5", "6.0"}
    );
}

// Test 21: Print multiple args
TEST_F(IntegrationTest, PrintMultipleArgs) {
    expect_output(
        "x = 42\n"
        "print(\"value:\", x)\n",
        {"value: 42"}
    );
}

// Test 22: None value
TEST_F(IntegrationTest, NoneValue) {
    expect_output(
        "x = None\n"
        "print(x)\n"
        "print(x == None)\n",
        {"None", "True"}
    );
}

// Test 23: Complex program - sum of squares
TEST_F(IntegrationTest, SumOfSquares) {
    expect_output(
        "def sum_squares(n):\n"
        "    total = 0\n"
        "    for i in range(1, n + 1):\n"
        "        total = total + i * i\n"
        "    return total\n"
        "print(sum_squares(5))\n",
        {"55"}
    );
}

// Test 24: Multiple functions
TEST_F(IntegrationTest, MultipleFunctions) {
    expect_output(
        "def is_even(n):\n"
        "    return n % 2 == 0\n"
        "def is_odd(n):\n"
        "    return n % 2 != 0\n"
        "print(is_even(4))\n"
        "print(is_odd(3))\n",
        {"True", "True"}
    );
}

// Test 25: For loop with range(2 args)
TEST_F(IntegrationTest, ForLoopRange2) {
    expect_output(
        "for i in range(2, 6):\n"
        "    print(i)\n",
        {"2", "3", "4", "5"}
    );
}

// Test 26: Chained comparisons
TEST_F(IntegrationTest, ChainedComparison) {
    expect_output(
        "x = 5\n"
        "print(x > 3 and x < 10)\n"
        "print(x > 10 or x > 3)\n",
        {"True", "True"}
    );
}

// Test 27: Recursive countdown
TEST_F(IntegrationTest, RecursiveCountdown) {
    expect_output(
        "def countdown(n):\n"
        "    if n <= 0:\n"
        "        return\n"
        "    print(n)\n"
        "    countdown(n - 1)\n"
        "countdown(3)\n",
        {"3", "2", "1"}
    );
}

// Test 28: Variable scoping
TEST_F(IntegrationTest, VariableScoping) {
    expect_output(
        "x = 1\n"
        "def f():\n"
        "    x = 2\n"
        "    print(x)\n"
        "f()\n"
        "print(x)\n",
        {"2", "1"}
    );
}

// Test 29: For loop accumulation
TEST_F(IntegrationTest, ForLoopAccumulation) {
    expect_output(
        "total = 0\n"
        "for i in range(1, 6):\n"
        "    total = total + i\n"
        "print(total)\n",
        {"15"}
    );
}

// Test 30: If with complex condition
TEST_F(IntegrationTest, ComplexCondition) {
    expect_output(
        "x = 5\n"
        "if x > 3 and x < 10:\n"
        "    print(\"in range\")\n"
        "else:\n"
        "    print(\"out of range\")\n",
        {"in range"}
    );
}
