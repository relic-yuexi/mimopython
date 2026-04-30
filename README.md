# mimopython

A lightweight Python subset interpreter built from scratch in C++20. Hand-written recursive descent parser, custom linear bytecode, stack-based virtual machine.

基于 C++20 从零构建的轻量级 Python 子集解释器。手写递归下降 Parser，自定义线性字节码，栈式虚拟机执行。

---

## Development Stats / 开发统计

| Metric | Value |
|--------|-------|
| Input tokens | 121.3k |
| Output tokens | 87.1k |
| Cost | $0.0530 |
| Context usage | 136.4k / 1M |
| Development time | 41 min 1 sec |
| Test pass rate | 119/119 (100%) |

> Built using **MiMo V2.5** (Xiaomi's language model) + **Claude Code** (Anthropic's CLI tool).
> Demonstrates that open-weight models paired with agentic coding tools can produce production-quality systems code end-to-end.

---

## Architecture / 架构设计

```
Source (.py)
    │
    ▼
┌─────────┐
│  Lexer   │  Tokenization → Token stream (INDENT / DEDENT)
└────┬────┘
     ▼
┌─────────┐
│  Parser  │  Recursive descent → AST
└────┬────┘
     ▼
┌──────────┐
│ Compiler  │  AST walk → Linear bytecode
└────┬─────┘
     ▼
┌─────────┐
│    VM    │  Stack machine execution → Output
└─────────┘
```

### Bytecode Instruction Set / 字节码指令集

| Instruction | Description |
|-------------|-------------|
| `LOAD_CONST` | Push constant onto stack |
| `LOAD_NAME` | Load variable by name |
| `STORE_NAME` | Store variable by name |
| `POP_TOP` | Pop top of stack |
| `BINARY_ADD/SUB/MUL/FLOOR_DIV/MOD` | Binary arithmetic |
| `UNARY_NEG/NOT` | Unary operators |
| `COMPARE_EQ/NEQ/LT/GT/LTE/GTE` | Comparison |
| `JUMP_IF_FALSE/TRUE` | Conditional jump |
| `JUMP_ABSOLUTE` | Unconditional jump |
| `CALL_FUNCTION` | Function call |
| `RETURN_VALUE` | Return from function |
| `PRINT` | Output (multiple args) |
| `HALT` | Stop execution |

### VM Design / 虚拟机设计

- **Operand stack**: `std::vector<PyValue>` for intermediate values
- **Call frame stack**: one frame per function call, with local variable table and return address
- **Variable resolution**: current frame → closure → global (lexical scope chain)
- **PyValue**: `std::variant<int64_t, double, bool, string, shared_ptr<PyFunction>>`

---

## Supported Syntax / 支持的语法

### Data Types / 数据类型
```python
x = 42          # int
y = 3.14        # float
b = True        # bool
s = "hello"     # str
n = None        # NoneType
```

### Arithmetic / 算术运算
```python
print(1 + 2 * 3)    # 7
print(10 // 3)       # 3
print(10 % 3)        # 1
print(-42)           # -42
```

### Comparison & Logic / 比较与逻辑
```python
print(1 < 2 and 3 > 2)   # True
print(not False)           # True
print(1 <= 1)              # True
```

### Control Flow / 控制流
```python
if x > 0:
    print("positive")
elif x < 0:
    print("negative")
else:
    print("zero")

while x > 0:
    x = x - 1

for i in range(10):
    print(i)

for i in range(2, 10):
    print(i)
```

### Functions / 函数
```python
def add(a, b):
    return a + b

def factorial(n):
    if n <= 1:
        return 1
    return n * factorial(n - 1)

print(add(3, 4))        # 7
print(factorial(10))     # 3628800
```

### String Operations / 字符串操作
```python
print("hello" + " " + "world")  # hello world
print("ab" * 3)                  # ababab
```

### break / continue / pass
```python
for i in range(10):
    if i == 3:
        break
    print(i)     # 0 1 2

for i in range(5):
    if i == 2:
        continue
    print(i)     # 0 1 3 4
```

---

## Project Structure / 项目结构

```
mimopython/
├── CMakeLists.txt
├── README.md
├── .gitignore
├── include/
│   ├── ast.h               # AST node definitions (Visitor pattern)
│   ├── bytecode.h          # Bytecode instruction set (30+ opcodes)
│   ├── compiler.h          # AST → Bytecode compiler
│   ├── lexer.h             # Tokenizer with indentation handling
│   ├── parser.h            # Recursive descent parser
│   ├── token.h             # Token type definitions
│   ├── value.h             # PyValue (std::variant wrapper)
│   └── vm.h                # Stack-based virtual machine
├── src/
│   ├── ast.cpp
│   ├── compiler.cpp
│   ├── lexer.cpp
│   ├── main.cpp            # CLI entry point
│   ├── parser.cpp
│   ├── value.cpp
│   └── vm.cpp
├── tests/
│   ├── test_lexer.cpp      # 19 tests
│   ├── test_parser.cpp     # 21 tests
│   ├── test_compiler.cpp   # 15 tests
│   ├── test_vm.cpp         # 34 tests
│   └── test_integration.cpp # 30 tests
└── test_programs/
    ├── hello.py
    ├── fibonacci.py
    ├── primes.py
    ├── control_flow.py
    └── functions.py
```

---

## Build & Run / 构建与运行

### Prerequisites / 环境要求

- MSYS2 MinGW64 (or any GCC 13+ / Clang 16+ with C++20 support)
- CMake 3.20+
- Ninja (or Make)
- GoogleTest

### Install Dependencies / 安装依赖

```bash
# MSYS2 MinGW64
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-gtest

# Ubuntu/Debian
sudo apt install cmake ninja-build libgtest-dev

# macOS (Homebrew)
brew install cmake ninja googletest
```

### Build / 构建

```bash
cmake -B build -G Ninja
cmake --build build
```

### Run Tests / 运行测试

```bash
cd build
ctest --output-on-failure
# or
./mimopython_tests
```

### Run a Program / 运行程序

```bash
./build/mimopython test_programs/hello.py
./build/mimopython test_programs/fibonacci.py
./build/mimopython test_programs/primes.py
```

---

## Benchmark: mimopython vs CPython / 性能对比

Test: compute `fib(30)` recursively.

测试：递归计算 `fib(30)`。

```bash
# mimopython
time ./build/mimopython test_programs/bench_fib.py

# CPython
time python3 test_programs/bench_fib.py
```

| Implementation | fib(30) | Notes |
|----------------|---------|-------|
| CPython 3.12 | 0.122s | Production interpreter, bytecode + eval loop |
| mimopython | 11.34s | Hand-written compiler + VM, no optimization pass |

mimopython is ~93x slower than CPython for deep recursion. This is expected: the VM has no optimization passes, no constant folding, and each function call saves/restores full VM state. The gap narrows significantly for compute-bound loops (where function call overhead is amortized).

mimopython 在深度递归场景下约为 CPython 的 1/93。这是预期之内的：VM 无优化 pass、无常量折叠、每次函数调用都保存/恢复完整 VM 状态。在计算密集型循环中差距会显著缩小。

---

## Test Results / 测试结果

```
[==========] Running 119 tests from 5 test suites.
[----------] 30 tests from IntegrationTest   → ALL PASSED
[----------] 34 tests from VmTest            → ALL PASSED
[----------] 15 tests from CompilerTest      → ALL PASSED
[----------] 21 tests from ParserTest        → ALL PASSED
[----------] 19 tests from LexerTest         → ALL PASSED
[==========] 119 tests ran. → 119 PASSED, 0 FAILED
```

| Module | Tests | Coverage |
|--------|-------|----------|
| Lexer | 19 | Tokenization, INDENT/DEDENT, keywords, operators, edge cases |
| Parser | 21 | All syntax constructs → AST |
| Compiler | 15 | Bytecode sequences, jump patching, function definitions |
| VM | 34 | Execution, scoping, recursion, loops, type operations |
| Integration | 30 | End-to-end programs with output verification |

---

## Key Issues Fixed During Development / 开发过程中修复的关键问题

1. **Lexer indentation**: Rewrote `handle_line_start` to measure indent level before skipping whitespace, correctly generating INDENT/DEDENT tokens
2. **Parser RANGE/PRINT**: `range` and `print` are keyword tokens; `parse_primary` needed to handle them as valid identifiers
3. **Compiler if-control-flow**: Fixed POP_TOP fall-through in single-branch if statements; always emit JUMP_ABSOLUTE to end after branch body
4. **Compiler for-loop continue**: `continue` must jump to the increment section (not loop start) to avoid infinite loops
5. **VM print argument order**: Arguments popped from stack are in reverse; must reverse before output
6. **VM variable scoping**: Function-local assignments must not modify outer scope variables
7. **PyValue boolean arithmetic**: Bool should be treated as numeric type, supporting `True + True == 2`

---

## Example Output / 示例输出

### hello.py
```
$ ./build/mimopython test_programs/hello.py
Hello, mimopython!
7
The answer is: 42
```

### fibonacci.py
```
$ ./build/mimopython test_programs/fibonacci.py
0
1
1
2
3
5
8
13
21
34
```

### primes.py
```
$ ./build/mimopython test_programs/primes.py
2
3
5
7
11
...
97
Total primes: 25
```

---

## Tech Stack / 技术栈

| Component | Technology |
|-----------|------------|
| Language | C++20 |
| Build | CMake 3.20+ + Ninja |
| Test | GoogleTest 1.17.0 |
| Compiler | GCC 15.2.0 (MSYS2 MinGW64) |
| Platform | Windows 11 / Linux / macOS |

---

## License

MIT

---

## Acknowledgments / 致谢

This project was built as a demonstration of **agentic AI-assisted development**.

本项目作为 **AI 代理辅助开发** 的演示而构建。

**Workflow**: Human writes specification → AI model generates code → Human reviews → AI iterates.

**工作流程**：人类编写规范 → AI 模型生成代码 → 人类审查 → AI 迭代。

- **AI Model**: [MiMo V2.5](https://platform.xiaomimimo.com?ref=B5TRBQ) — Xiaomi's open-weight language model
- **Coding Agent**: [Claude Code](https://claude.ai/code) — Anthropic's CLI-based coding assistant
- **Total cost**: $0.0530 | **Total time**: 41 minutes | **Tests**: 119/119 passing

> If you want to try MiMo, use referral code **B5TRBQ** when signing up at
> [platform.xiaomimimo.com](https://platform.xiaomimimo.com?ref=B5TRBQ)
> to get $2 API credit (valid for 40 days).
