/**
 * @file main.cpp
 * @brief CLI entry point for the mimopython interpreter.
 *
 * Usage: mimopython <file.py>
 * Reads a .py file and executes it through the compiler pipeline.
 */
#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "vm.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: mimopython <file.py>" << std::endl;
        return 1;
    }

    std::string filename = argv[1];
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: cannot open file '" << filename << "'" << std::endl;
        return 1;
    }

    // Get directory of the script (for resolving imports)
    std::string script_dir;
    try {
        script_dir = std::filesystem::path(filename).parent_path().string();
    } catch (...) {}

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string source = ss.str();

    if (!source.empty() && source.back() != '\n') {
        source += '\n';
    }

    try {
        mimo::Lexer lexer(source);
        auto tokens = lexer.tokenize();
        if (lexer.has_errors()) {
            std::cerr << "Lexer errors:" << std::endl;
            for (auto& err : lexer.errors()) {
                std::cerr << "  Line " << err.line << ": " << err.value << std::endl;
            }
            return 1;
        }

        mimo::Parser parser(std::move(tokens));
        auto ast = parser.parse();
        if (parser.has_errors()) {
            std::cerr << "Parse errors:" << std::endl;
            for (auto& err : parser.errors()) {
                std::cerr << "  Line " << err.line << ": " << err.what() << std::endl;
            }
            return 1;
        }

        mimo::Compiler compiler;
        auto code = compiler.compile(ast);

        mimo::Vm vm;
        vm.set_script_dir(script_dir);
        vm.execute(code);

    } catch (const mimo::RuntimeError& e) {
        std::cerr << "RuntimeError: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
