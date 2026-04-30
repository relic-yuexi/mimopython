/**
 * @file token.h
 * @brief Token types and Token struct for the lexer.
 */
#pragma once

#include <string>
#include <cstdint>
#include <iostream>

namespace mimo {

enum class TokenType {
    // Literals
    INTEGER, FLOAT, STRING, TRUE, FALSE, NONE,

    // Identifiers & keywords
    IDENTIFIER,
    DEF, RETURN, IF, ELIF, ELSE, WHILE, FOR, IN, AND, OR, NOT,
    PRINT, RANGE, BREAK, CONTINUE, PASS,

    // Operators
    PLUS, MINUS, STAR, SLASH, SLASHSLASH, PERCENT,
    EQ, NEQ, LT, GT, LTE, GTE,
    ASSIGN,
    LPAREN, RPAREN, COMMA, COLON,

    // Structure
    NEWLINE, INDENT, DEDENT, ENDOFFILE,

    // Error
    ERROR
};

struct Token {
    TokenType type;
    std::string value;
    uint32_t line;
    uint32_t column;

    Token() : type(TokenType::ERROR), line(0), column(0) {}
    Token(TokenType t, std::string v, uint32_t l, uint32_t c)
        : type(t), value(std::move(v)), line(l), column(c) {}

    bool is_error() const { return type == TokenType::ERROR; }
};

inline const char* token_type_name(TokenType t) {
    switch (t) {
        case TokenType::INTEGER:    return "INTEGER";
        case TokenType::FLOAT:      return "FLOAT";
        case TokenType::STRING:     return "STRING";
        case TokenType::TRUE:       return "TRUE";
        case TokenType::FALSE:      return "FALSE";
        case TokenType::NONE:       return "NONE";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::DEF:        return "DEF";
        case TokenType::RETURN:     return "RETURN";
        case TokenType::IF:         return "IF";
        case TokenType::ELIF:       return "ELIF";
        case TokenType::ELSE:       return "ELSE";
        case TokenType::WHILE:      return "WHILE";
        case TokenType::FOR:        return "FOR";
        case TokenType::IN:         return "IN";
        case TokenType::AND:        return "AND";
        case TokenType::OR:         return "OR";
        case TokenType::NOT:        return "NOT";
        case TokenType::PRINT:      return "PRINT";
        case TokenType::RANGE:      return "RANGE";
        case TokenType::BREAK:      return "BREAK";
        case TokenType::CONTINUE:   return "CONTINUE";
        case TokenType::PASS:       return "PASS";
        case TokenType::PLUS:       return "PLUS";
        case TokenType::MINUS:      return "MINUS";
        case TokenType::STAR:       return "STAR";
        case TokenType::SLASH:      return "SLASH";
        case TokenType::SLASHSLASH: return "SLASHSLASH";
        case TokenType::PERCENT:    return "PERCENT";
        case TokenType::EQ:         return "EQ";
        case TokenType::NEQ:        return "NEQ";
        case TokenType::LT:         return "LT";
        case TokenType::GT:         return "GT";
        case TokenType::LTE:        return "LTE";
        case TokenType::GTE:        return "GTE";
        case TokenType::ASSIGN:     return "ASSIGN";
        case TokenType::LPAREN:     return "LPAREN";
        case TokenType::RPAREN:     return "RPAREN";
        case TokenType::COMMA:      return "COMMA";
        case TokenType::COLON:      return "COLON";
        case TokenType::NEWLINE:    return "NEWLINE";
        case TokenType::INDENT:     return "INDENT";
        case TokenType::DEDENT:     return "DEDENT";
        case TokenType::ENDOFFILE:  return "EOF";
        case TokenType::ERROR:      return "ERROR";
    }
    return "UNKNOWN";
}

} // namespace mimo
