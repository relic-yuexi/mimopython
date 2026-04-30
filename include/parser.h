/**
 * @file parser.h
 * @brief Recursive descent parser: Token stream -> AST.
 *
 * Parses the token stream produced by Lexer into an AST.
 * Handles Python-style indentation blocks, operator precedence,
 * and all supported syntax constructs.
 */
#pragma once

#include "token.h"
#include "ast.h"
#include <vector>
#include <string>
#include <stdexcept>
#include <cstdint>

namespace mimo {

class ParseError : public std::runtime_error {
public:
    uint32_t line;
    ParseError(const std::string& msg, uint32_t ln)
        : std::runtime_error(msg), line(ln) {}
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    ProgramNode parse();
    bool has_errors() const { return has_errors_; }
    const std::vector<ParseError>& errors() const { return errors_; }

private:
    std::vector<Token> tokens_;
    size_t pos_ = 0;
    bool has_errors_ = false;
    std::vector<ParseError> errors_;

    // Token navigation
    const Token& peek() const;
    const Token& advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    const Token& expect(TokenType type, const std::string& message);
    bool is_at_end() const;
    void skip_newlines();

    // Parsing rules
    std::vector<StmtPtr> parse_block();
    std::vector<StmtPtr> parse_block_body();

    StmtPtr parse_statement();
    StmtPtr parse_assignment_or_expr();
    StmtPtr parse_if();
    StmtPtr parse_while();
    StmtPtr parse_for();
    StmtPtr parse_def();
    StmtPtr parse_return();
    StmtPtr parse_print();

    // Expression parsing (precedence climbing)
    ExprPtr parse_expression();
    ExprPtr parse_or();
    ExprPtr parse_and();
    ExprPtr parse_not();
    ExprPtr parse_comparison();
    ExprPtr parse_addition();
    ExprPtr parse_multiplication();
    ExprPtr parse_unary();
    ExprPtr parse_call();
    ExprPtr parse_primary();

    // Error recovery
    ParseError make_error(const std::string& msg);
    void synchronize();
};

} // namespace mimo
