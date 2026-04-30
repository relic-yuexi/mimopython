/**
 * @file lexer.h
 * @brief Lexer (tokenizer) with Python-style indentation handling.
 *
 * Converts source text into a Token stream. Manages INDENT/DEDENT tokens
 * by tracking an indentation level stack. Tabs are converted to 4 spaces.
 */
#pragma once

#include "token.h"
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace mimo {

class Lexer {
public:
    explicit Lexer(std::string_view source);
    std::vector<Token> tokenize();
    bool has_errors() const { return has_errors_; }
    const std::vector<Token>& errors() const { return error_tokens_; }

private:
    std::string source_;
    uint32_t pos_ = 0;
    uint32_t line_ = 1;
    uint32_t col_ = 1;
    bool at_line_start_ = true;
    std::vector<uint32_t> indent_stack_{0};
    bool has_errors_ = false;
    std::vector<Token> error_tokens_;
    uint32_t paren_depth_ = 0; // track parentheses for implicit line continuation

    static const std::unordered_map<std::string, TokenType> keywords_;

    char peek() const;
    char advance();
    bool match(char expected);
    bool is_at_end() const;
    void skip_inline_spaces();

    Token make_token(TokenType type, std::string value);
    Token error_token(std::string message);

    Token read_number();
    Token read_string(char quote);
    Token read_identifier();
};

} // namespace mimo
