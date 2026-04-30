/**
 * @file lexer.cpp
 * @brief Lexer implementation with Python-style indentation handling.
 */
#include "lexer.h"
#include <cctype>

namespace mimo {

const std::unordered_map<std::string, TokenType> Lexer::keywords_ = {
    {"def", TokenType::DEF}, {"return", TokenType::RETURN},
    {"if", TokenType::IF}, {"elif", TokenType::ELIF}, {"else", TokenType::ELSE},
    {"while", TokenType::WHILE}, {"for", TokenType::FOR}, {"in", TokenType::IN},
    {"and", TokenType::AND}, {"or", TokenType::OR}, {"not", TokenType::NOT},
    {"print", TokenType::PRINT}, {"range", TokenType::RANGE},
    {"True", TokenType::TRUE}, {"False", TokenType::FALSE}, {"None", TokenType::NONE},
    {"break", TokenType::BREAK}, {"continue", TokenType::CONTINUE}, {"pass", TokenType::PASS},
};

Lexer::Lexer(std::string_view source) : source_(std::string(source)) {}

char Lexer::peek() const {
    if (pos_ >= source_.size()) return '\0';
    return source_[pos_];
}

char Lexer::advance() {
    char c = source_[pos_++];
    if (c == '\n') { line_++; col_ = 1; }
    else { col_++; }
    return c;
}

bool Lexer::match(char expected) {
    if (pos_ >= source_.size() || source_[pos_] != expected) return false;
    advance();
    return true;
}

bool Lexer::is_at_end() const { return pos_ >= source_.size(); }

void Lexer::skip_inline_spaces() {
    while (!is_at_end() && (peek() == ' ' || peek() == '\t' || peek() == '\r')) advance();
}

Token Lexer::make_token(TokenType type, std::string value) {
    return Token(type, std::move(value), line_, col_);
}

Token Lexer::error_token(std::string message) {
    has_errors_ = true;
    Token tok(TokenType::ERROR, std::move(message), line_, col_);
    error_tokens_.push_back(tok);
    return tok;
}

Token Lexer::read_number() {
    uint32_t start_col = col_, start_line = line_;
    std::string num;
    bool is_float = false;
    while (!is_at_end() && (std::isdigit(peek()) || peek() == '.')) {
        if (peek() == '.') { if (is_float) break; is_float = true; }
        num += advance();
    }
    return Token(is_float ? TokenType::FLOAT : TokenType::INTEGER, std::move(num), start_line, start_col);
}

Token Lexer::read_string(char quote) {
    uint32_t start_col = col_ - 1, start_line = line_;
    std::string str;
    while (!is_at_end() && peek() != quote) {
        if (peek() == '\\') {
            advance();
            if (is_at_end()) return error_token("unterminated string escape");
            char esc = advance();
            switch (esc) {
                case 'n': str += '\n'; break; case 't': str += '\t'; break;
                case '\\': str += '\\'; break; case '\'': str += '\''; break;
                case '"': str += '"'; break; default: str += esc; break;
            }
        } else if (peek() == '\n') {
            return error_token("unterminated string literal");
        } else {
            str += advance();
        }
    }
    if (is_at_end()) return error_token("unterminated string literal");
    advance();
    return Token(TokenType::STRING, std::move(str), start_line, start_col);
}

Token Lexer::read_identifier() {
    uint32_t start_col = col_, start_line = line_;
    std::string id;
    while (!is_at_end() && (std::isalnum(peek()) || peek() == '_')) id += advance();
    auto it = keywords_.find(id);
    if (it != keywords_.end()) return Token(it->second, std::move(id), start_line, start_col);
    return Token(TokenType::IDENTIFIER, std::move(id), start_line, start_col);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    bool at_line_start = true;

    while (!is_at_end()) {
        // Handle indentation at line start
        if (at_line_start) {
            at_line_start = false;

            // Skip blank lines and measure indentation
            while (!is_at_end()) {
                // Skip spaces and tabs (counting indentation)
                uint32_t indent = 0;
                while (!is_at_end() && (peek() == ' ' || peek() == '\t')) {
                    if (peek() == ' ') indent++;
                    else indent += 4;
                    advance();
                }
                // Skip carriage returns
                while (!is_at_end() && peek() == '\r') advance();

                if (is_at_end()) break;

                if (peek() == '\n') {
                    advance(); // skip blank line
                    continue;
                }
                if (peek() == '#') {
                    while (!is_at_end() && peek() != '\n') advance();
                    if (!is_at_end()) advance(); // skip newline after comment
                    continue;
                }

                // This is a non-blank, non-comment line
                uint32_t current = indent_stack_.back();
                if (indent > current) {
                    indent_stack_.push_back(indent);
                    tokens.push_back(make_token(TokenType::INDENT, ""));
                } else if (indent < current) {
                    while (indent_stack_.size() > 1 && indent_stack_.back() > indent) {
                        indent_stack_.pop_back();
                        tokens.push_back(make_token(TokenType::DEDENT, ""));
                    }
                    if (indent_stack_.back() != indent) {
                        error_token("inconsistent indentation");
                    }
                }
                break; // done with indentation handling
            }
        }

        if (is_at_end()) break;

        // Skip inline spaces (not at line start)
        skip_inline_spaces();
        if (is_at_end()) break;

        char c = peek();

        // Newline
        if (c == '\n') {
            advance();
            if (!paren_depth_) {
                tokens.push_back(make_token(TokenType::NEWLINE, "\\n"));
                at_line_start = true;
            }
            continue;
        }

        // Comment
        if (c == '#') {
            while (!is_at_end() && peek() != '\n') advance();
            continue;
        }

        // Numbers
        if (std::isdigit(c)) { tokens.push_back(read_number()); continue; }

        // Strings
        if (c == '"' || c == '\'') { advance(); tokens.push_back(read_string(c)); continue; }

        // Identifiers and keywords
        if (std::isalpha(c) || c == '_') { tokens.push_back(read_identifier()); continue; }

        // Operators and punctuation
        advance();
        switch (c) {
            case '+': tokens.push_back(make_token(TokenType::PLUS, "+")); break;
            case '-': tokens.push_back(make_token(TokenType::MINUS, "-")); break;
            case '*': tokens.push_back(make_token(TokenType::STAR, "*")); break;
            case '%': tokens.push_back(make_token(TokenType::PERCENT, "%")); break;
            case '(': paren_depth_++; tokens.push_back(make_token(TokenType::LPAREN, "(")); break;
            case ')':
                if (paren_depth_ > 0) paren_depth_--;
                tokens.push_back(make_token(TokenType::RPAREN, ")"));
                break;
            case ',': tokens.push_back(make_token(TokenType::COMMA, ",")); break;
            case ':': tokens.push_back(make_token(TokenType::COLON, ":")); break;
            case '/':
                if (match('/')) tokens.push_back(make_token(TokenType::SLASHSLASH, "//"));
                else tokens.push_back(make_token(TokenType::SLASH, "/"));
                break;
            case '=':
                if (match('=')) tokens.push_back(make_token(TokenType::EQ, "=="));
                else tokens.push_back(make_token(TokenType::ASSIGN, "="));
                break;
            case '!':
                if (match('=')) tokens.push_back(make_token(TokenType::NEQ, "!="));
                else tokens.push_back(error_token("unexpected character '!'"));
                break;
            case '<':
                if (match('=')) tokens.push_back(make_token(TokenType::LTE, "<="));
                else tokens.push_back(make_token(TokenType::LT, "<"));
                break;
            case '>':
                if (match('=')) tokens.push_back(make_token(TokenType::GTE, ">="));
                else tokens.push_back(make_token(TokenType::GT, ">"));
                break;
            default:
                tokens.push_back(error_token(std::string("unexpected character '") + c + "'"));
                break;
        }
    }

    // Generate remaining DEDENTs
    while (indent_stack_.size() > 1) {
        indent_stack_.pop_back();
        tokens.push_back(make_token(TokenType::DEDENT, ""));
    }

    tokens.push_back(make_token(TokenType::ENDOFFILE, ""));
    return tokens;
}

} // namespace mimo
