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

> Built using **MiMo V2.5 Pro** (Xiaomi's language model) + **Claude Code** (Anthropic's CLI tool).
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

### Modules / 模块导入
```python
# Import entire module (all names merged into namespace)
import mymath
print(square(5))    # uses mymath.square

# Import specific name
from mymath import square
print(square(7))    # 49
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

## Quick Start / 快速开始

### Windows (MSYS2) — 从零开始

#### 第一步：安装 MSYS2

1. 下载 MSYS2 安装包：https://www.msys2.org/
2. 运行安装程序，安装到默认路径 `C:\msys64`
3. 安装完成后，打开 **MSYS2 MinGW64** 终端（不是 MSYS2 MSYS）

> **重要**：必须使用 **MinGW64** 终端，不是 MSYS 终端。开始菜单搜索 "MSYS2 MinGW64"。

#### 第二步：安装工具链和依赖

在 MSYS2 MinGW64 终端中执行：

```bash
# 更新包管理器（首次使用需要）
pacman -Syu

# 安装编译工具链
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-gtest
```

全部选 Y 确认安装。

#### 第三步：克隆并编译

```bash
# 克隆项目
git clone https://github.com/YOUR_USERNAME/mimopython.git
cd mimopython

# 配置构建
cmake -B build -G Ninja

# 编译
cmake --build build
```

编译成功后你会看到：

```
[5/5] Linking CXX executable mimopython_tests.exe
```

#### 第四步：运行测试

```bash
cd build
./mimopython_tests.exe
```

预期输出：

```
[==========] Running 119 tests from 5 test suites.
[  PASSED  ] 119 tests.
```

#### 第五步：运行你的第一个程序

```bash
cd build
./mimopython.exe ../test_programs/hello.py
```

预期输出：

```
Hello, mimopython!
7
The answer is: 42
```

#### 第六步：写你自己的 Python 程序

创建一个文件 `my_program.py`：

```python
def greet(name):
    print("Hello, " + name + "!")

def fibonacci(n):
    a = 0
    b = 1
    for i in range(n):
        print(a)
        temp = a + b
        a = b
        b = temp

greet("World")
print("---")
fibonacci(10)
```

运行：

```bash
./build/mimopython my_program.py
```

---

### Ubuntu / Debian — 从零开始

```bash
# 1. 安装依赖
sudo apt update
sudo apt install -y build-essential cmake ninja-build libgtest-dev git

# 2. 克隆项目
git clone https://github.com/YOUR_USERNAME/mimopython.git
cd mimopython

# 3. 编译
cmake -B build -G Ninja
cmake --build build

# 4. 运行测试
cd build
./mimopython_tests

# 5. 运行示例程序
./mimopython ../test_programs/hello.py
```

---

### macOS — 从零开始

```bash
# 1. 安装 Xcode Command Line Tools（如果没有的话）
xcode-select --install

# 2. 安装 Homebrew（如果没有的话）
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 3. 安装依赖
brew install cmake ninja googletest

# 4. 克隆项目
git clone https://github.com/YOUR_USERNAME/mimopython.git
cd mimopython

# 5. 编译
cmake -B build -G Ninja
cmake --build build

# 6. 运行测试
cd build
./mimopython_tests

# 7. 运行示例程序
./mimopython ../test_programs/hello.py
```

---

### 常见问题 / Troubleshooting

#### Q: `cmake: command not found`

确保 cmake 在 PATH 中。MSYS2 用户需要安装 `mingw-w64-x86_64-cmake`（不是 `cmake`）。

#### Q: `ninja: command not found`

同上，MSYS2 用户安装 `mingw-w64-x86_64-ninja`。或者用 Make 替代：

```bash
cmake -B build
cmake --build build
```

#### Q: `fatal error: gtest/gtest.h: No such file or directory`

GoogleTest 没安装。MSYS2 执行 `pacman -S mingw-w64-x86_64-gtest`，Ubuntu 执行 `sudo apt install libgtest-dev`。

#### Q: 编译报错 `use of an operand of type 'bool' in 'operator++' is forbidden`

你的 GCC 版本太旧。本项目需要 GCC 13+ 或 Clang 16+（支持 C++20）。MSYS2 用户执行 `pacman -Syu` 更新工具链。

#### Q: `Permission denied` 链接错误

Windows 上可能有杀毒软件锁定了 exe 文件。关闭杀毒软件实时保护后重试，或在另一个目录重新构建。

#### Q: 如何使用 Make 而不是 Ninja？

```bash
cmake -B build          # 不加 -G Ninja，默认使用 Make
cmake --build build
```

#### Q: 如何启用 AddressSanitizer（内存检测）？

```bash
cmake -B build -DENABLE_ASAN=ON
cmake --build build
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

| Test | CPython 3.12 | mimopython (Release) | Ratio |
|------|-------------|---------------------|-------|
| fib(25) | 6.86 ± 0.21ms | 41.42 ± 0.70ms | 6.0x |
| fib(30) | 77.1 ± 2.65ms | 1160 ± 427ms | 15.1x |
| factorial(100) | 0.03 ± 0.01ms | 0.10 ± 0.05ms | 3.3x |
| ackermann(3,6) | 13.3 ± 0.48ms | 118.5 ± 16.5ms | 8.9x |
| primes<500 | 0.15 ± 0.01ms | 0.73 ± 0.31ms | 4.9x |
| loop 1M | 47.4 ± 1.64ms | 114.3 ± 12.2ms | 2.4x |
| while 2M | 151.0 ± 53.7ms | 259.0 ± 26.3ms | 1.7x |
| nested 100x100 | 1.67 ± 0.17ms | 1.01 ± 0.36ms | **0.6x** |
| string concat | 0.16 ± 0.09ms | 0.26 ± 0.20ms | 1.6x |

> 10 runs per test, mean ± std. Release build with `-O3`.
> See [CHANGELOG.md](CHANGELOG.md) for the full optimization journey.

经过 4 轮优化后（Release -O3），mimopython 在大多数场景下为 CPython 的 2-15 倍，部分场景持平甚至更快。详见 [CHANGELOG.md](CHANGELOG.md)。

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

- **AI Model**: [MiMo V2.5 Pro](https://platform.xiaomimimo.com?ref=B5TRBQ) — Xiaomi's open-weight language model
- **Coding Agent**: [Claude Code](https://claude.ai/code) — Anthropic's CLI-based coding assistant
- **Total cost**: $0.0530 | **Total time**: 41 minutes | **Tests**: 119/119 passing

> If you want to try MiMo, use referral code **B5TRBQ** when signing up at
> [platform.xiaomimimo.com](https://platform.xiaomimimo.com?ref=B5TRBQ)
> to get $2 API credit (valid for 40 days).
