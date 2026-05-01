cmake_minimum_required(VERSION 3.20)
project(mimopython LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

# --- GoogleTest ---
find_package(GTest REQUIRED)

# --- Library: mimopython_core ---
add_library(mimopython_core STATIC
    src/ast.cpp
    src/lexer.cpp
    src/parser.cpp
    src/compiler.cpp
    src/vm.cpp
    src/value.cpp
    src/jit.cpp
)
target_include_directories(mimopython_core PUBLIC include)

# --- Main executable ---
add_executable(mimopython src/main.cpp)
target_link_libraries(mimopython PRIVATE mimopython_core)

# --- Tests ---
enable_testing()

add_executable(mimopython_tests
    tests/test_lexer.cpp
    tests/test_parser.cpp
    tests/test_compiler.cpp
    tests/test_vm.cpp
    tests/test_integration.cpp
)
target_link_libraries(mimopython_tests PRIVATE mimopython_core GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(mimopython_tests)
