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

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string source = ss.str();

    // Add trailing newline if missing
    if (!source.empty() && source.back() != '\n') {
        source += '\n';
    }

    try {
        // Lex
        mimo::Lexer lexer(source);
        auto tokens = lexer.tokenize();
        if (lexer.has_errors()) {
            std::cerr << "Lexer errors:" << std::endl;
            for (auto& err : lexer.errors()) {
                std::cerr << "  Line " << err.line << ": " << err.value << std::endl;
            }
            return 1;
        }

        // Parse
        mimo::Parser parser(std::move(tokens));
        auto ast = parser.parse();
        if (parser.has_errors()) {
            std::cerr << "Parse errors:" << std::endl;
            for (auto& err : parser.errors()) {
                std::cerr << "  Line " << err.line << ": " << err.what() << std::endl;
            }
            return 1;
        }

        // Compile
        mimo::Compiler compiler;
        auto code = compiler.compile(ast);

        // Execute
        mimo::Vm vm;
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
