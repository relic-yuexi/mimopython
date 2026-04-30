/**
 * @file parser.cpp
 * @brief Recursive descent parser implementation.
 */
#include "parser.h"
#include <sstream>

namespace mimo {

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

ProgramNode Parser::parse() {
    ProgramNode program;
    skip_newlines();
    while (!is_at_end()) {
        try {
            auto stmt = parse_statement();
            if (stmt) program.statements.push_back(std::move(stmt));
        } catch (const ParseError& e) {
            has_errors_ = true;
            errors_.push_back(e);
            synchronize();
        }
        skip_newlines();
    }
    return program;
}

// --- Token navigation ---

const Token& Parser::peek() const {
    return tokens_[pos_];
}

const Token& Parser::advance() {
    if (!is_at_end()) pos_++;
    return tokens_[pos_ - 1];
}

bool Parser::check(TokenType type) const {
    if (is_at_end()) return false;
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

const Token& Parser::expect(TokenType type, const std::string& message) {
    if (check(type)) return advance();
    throw make_error(message + " at line " + std::to_string(peek().line));
}

bool Parser::is_at_end() const {
    return pos_ >= tokens_.size() || peek().type == TokenType::ENDOFFILE;
}

void Parser::skip_newlines() {
    while (check(TokenType::NEWLINE)) advance();
}

ParseError Parser::make_error(const std::string& msg) {
    return ParseError(msg, peek().line);
}

void Parser::synchronize() {
    while (!is_at_end()) {
        if (peek().type == TokenType::NEWLINE) {
            advance();
            return;
        }
        switch (peek().type) {
            case TokenType::DEF:
            case TokenType::IF:
            case TokenType::WHILE:
            case TokenType::FOR:
            case TokenType::RETURN:
            case TokenType::PRINT:
            case TokenType::BREAK:
            case TokenType::CONTINUE:
            case TokenType::IMPORT:
            case TokenType::FROM:
                return;
            default:
                advance();
                break;
        }
    }
}

// --- Block parsing ---

std::vector<StmtPtr> Parser::parse_block() {
    std::vector<StmtPtr> stmts;
    skip_newlines();
    if (!check(TokenType::INDENT)) {
        throw make_error("expected indented block");
    }
    advance(); // consume INDENT
    skip_newlines();

    while (!is_at_end() && !check(TokenType::DEDENT)) {
        auto stmt = parse_statement();
        if (stmt) stmts.push_back(std::move(stmt));
        skip_newlines();
    }

    if (check(TokenType::DEDENT)) advance();
    return stmts;
}

// --- Statement parsing ---

StmtPtr Parser::parse_statement() {
    skip_newlines();
    if (is_at_end()) return nullptr;

    if (check(TokenType::IF)) return parse_if();
    if (check(TokenType::WHILE)) return parse_while();
    if (check(TokenType::FOR)) return parse_for();
    if (check(TokenType::DEF)) return parse_def();
    if (check(TokenType::RETURN)) return parse_return();
    if (check(TokenType::PRINT)) return parse_print();
    if (check(TokenType::BREAK)) { auto l = peek().line; advance(); return std::make_unique<BreakStmt>(l); }
    if (check(TokenType::CONTINUE)) { auto l = peek().line; advance(); return std::make_unique<ContinueStmt>(l); }
    if (check(TokenType::PASS)) { auto l = peek().line; advance(); return std::make_unique<PassStmt>(l); }
    if (check(TokenType::IMPORT)) return parse_import();
    if (check(TokenType::FROM)) return parse_from_import();

    return parse_assignment_or_expr();
}

StmtPtr Parser::parse_assignment_or_expr() {
    auto expr = parse_expression();
    uint32_t ln = expr->line;

    if (check(TokenType::ASSIGN)) {
        advance(); // consume =
        auto id = dynamic_cast<Identifier*>(expr.get());
        if (!id) throw make_error("invalid assignment target");
        std::string name = id->name;
        expr.release();
        auto val = parse_expression();
        return std::make_unique<AssignStmt>(std::move(name), std::move(val), ln);
    }

    return std::make_unique<ExprStmt>(std::move(expr), ln);
}

StmtPtr Parser::parse_if() {
    auto if_stmt = std::make_unique<IfStmt>(peek().line);
    advance(); // consume 'if'

    auto cond = parse_expression();
    expect(TokenType::COLON, "expected ':'");
    auto body = parse_block();

    IfStmt::Branch if_branch;
    if_branch.condition = std::move(cond);
    if_branch.body = std::move(body);
    if_stmt->branches.push_back(std::move(if_branch));

    skip_newlines();

    // elif branches
    while (check(TokenType::ELIF)) {
        advance();
        auto elif_cond = parse_expression();
        expect(TokenType::COLON, "expected ':'");
        auto elif_body = parse_block();

        IfStmt::Branch elif_branch;
        elif_branch.condition = std::move(elif_cond);
        elif_branch.body = std::move(elif_body);
        if_stmt->branches.push_back(std::move(elif_branch));
        skip_newlines();
    }

    // else branch
    if (check(TokenType::ELSE)) {
        advance();
        expect(TokenType::COLON, "expected ':'");
        auto else_body = parse_block();

        IfStmt::Branch else_branch;
        else_branch.condition = nullptr; // no condition = else
        else_branch.body = std::move(else_body);
        if_stmt->branches.push_back(std::move(else_branch));
    }

    return if_stmt;
}

StmtPtr Parser::parse_while() {
    auto ln = peek().line;
    advance(); // consume 'while'
    auto cond = parse_expression();
    expect(TokenType::COLON, "expected ':'");
    auto body = parse_block();
    return std::make_unique<WhileStmt>(std::move(cond), std::move(body), ln);
}

StmtPtr Parser::parse_for() {
    auto ln = peek().line;
    advance(); // consume 'for'
    auto& name_tok = expect(TokenType::IDENTIFIER, "expected variable name");
    std::string var = name_tok.value;
    expect(TokenType::IN, "expected 'in'");
    auto iterable = parse_expression();
    expect(TokenType::COLON, "expected ':'");
    auto body = parse_block();
    return std::make_unique<ForStmt>(std::move(var), std::move(iterable), std::move(body), ln);
}

StmtPtr Parser::parse_def() {
    auto ln = peek().line;
    advance(); // consume 'def'
    auto& name_tok = expect(TokenType::IDENTIFIER, "expected function name");
    std::string name = name_tok.value;
    expect(TokenType::LPAREN, "expected '('");

    std::vector<std::string> params;
    if (!check(TokenType::RPAREN)) {
        do {
            auto& param = expect(TokenType::IDENTIFIER, "expected parameter name");
            params.push_back(param.value);
        } while (match(TokenType::COMMA));
    }
    expect(TokenType::RPAREN, "expected ')'");
    expect(TokenType::COLON, "expected ':'");

    auto body = parse_block();
    return std::make_unique<FuncDef>(std::move(name), std::move(params), std::move(body), ln);
}

StmtPtr Parser::parse_return() {
    auto ln = peek().line;
    advance(); // consume 'return'
    ExprPtr value = nullptr;
    if (!check(TokenType::NEWLINE) && !check(TokenType::DEDENT) && !check(TokenType::ENDOFFILE)) {
        value = parse_expression();
    }
    return std::make_unique<ReturnStmt>(std::move(value), ln);
}

StmtPtr Parser::parse_print() {
    auto ln = peek().line;
    advance(); // consume 'print'
    expect(TokenType::LPAREN, "expected '(' after print");
    std::vector<ExprPtr> args;
    if (!check(TokenType::RPAREN)) {
        do {
            args.push_back(parse_expression());
        } while (match(TokenType::COMMA));
    }
    expect(TokenType::RPAREN, "expected ')'");
    return std::make_unique<PrintStmt>(std::move(args), ln);
}

StmtPtr Parser::parse_import() {
    auto ln = peek().line;
    advance(); // consume 'import'
    auto& mod = expect(TokenType::IDENTIFIER, "expected module name");
    std::string alias;
    if (match(TokenType::AS)) {
        alias = expect(TokenType::IDENTIFIER, "expected alias name").value;
    }
    return std::make_unique<ImportStmt>(mod.value, std::move(alias), ln);
}

StmtPtr Parser::parse_from_import() {
    auto ln = peek().line;
    advance(); // consume 'from'
    auto& mod = expect(TokenType::IDENTIFIER, "expected module name");
    expect(TokenType::IMPORT, "expected 'import'");
    auto& name = expect(TokenType::IDENTIFIER, "expected name to import");
    return std::make_unique<FromImportStmt>(mod.value, name.value, ln);
}

// --- Expression parsing (precedence climbing) ---

ExprPtr Parser::parse_expression() {
    return parse_or();
}

ExprPtr Parser::parse_or() {
    auto left = parse_and();
    while (check(TokenType::OR)) {
        auto ln = peek().line;
        advance();
        auto right = parse_and();
        left = std::make_unique<BinaryExpr>(TokenType::OR, std::move(left), std::move(right), ln);
    }
    return left;
}

ExprPtr Parser::parse_and() {
    auto left = parse_not();
    while (check(TokenType::AND)) {
        auto ln = peek().line;
        advance();
        auto right = parse_not();
        left = std::make_unique<BinaryExpr>(TokenType::AND, std::move(left), std::move(right), ln);
    }
    return left;
}

ExprPtr Parser::parse_not() {
    if (check(TokenType::NOT)) {
        auto ln = peek().line;
        advance();
        auto operand = parse_not();
        return std::make_unique<UnaryExpr>(TokenType::NOT, std::move(operand), ln);
    }
    return parse_comparison();
}

ExprPtr Parser::parse_comparison() {
    auto left = parse_addition();
    while (check(TokenType::EQ) || check(TokenType::NEQ) ||
           check(TokenType::LT) || check(TokenType::GT) ||
           check(TokenType::LTE) || check(TokenType::GTE)) {
        auto op = peek().type;
        auto ln = peek().line;
        advance();
        auto right = parse_addition();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right), ln);
    }
    return left;
}

ExprPtr Parser::parse_addition() {
    auto left = parse_multiplication();
    while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
        auto op = peek().type;
        auto ln = peek().line;
        advance();
        auto right = parse_multiplication();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right), ln);
    }
    return left;
}

ExprPtr Parser::parse_multiplication() {
    auto left = parse_unary();
    while (check(TokenType::STAR) || check(TokenType::SLASH) ||
           check(TokenType::SLASHSLASH) || check(TokenType::PERCENT)) {
        auto op = peek().type;
        auto ln = peek().line;
        advance();
        auto right = parse_unary();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right), ln);
    }
    return left;
}

ExprPtr Parser::parse_unary() {
    if (check(TokenType::MINUS)) {
        auto ln = peek().line;
        advance();
        auto operand = parse_unary();
        return std::make_unique<UnaryExpr>(TokenType::MINUS, std::move(operand), ln);
    }
    return parse_call();
}

ExprPtr Parser::parse_call() {
    auto expr = parse_primary();

    // Check for function call: identifier followed by '('
    if (auto id = dynamic_cast<Identifier*>(expr.get())) {
        if (check(TokenType::LPAREN)) {
            advance(); // consume '('
            std::vector<ExprPtr> args;
            if (!check(TokenType::RPAREN)) {
                do {
                    args.push_back(parse_expression());
                } while (match(TokenType::COMMA));
            }
            expect(TokenType::RPAREN, "expected ')'");
            std::string callee = id->name;
            uint32_t ln = expr->line;
            expr.release();
            return std::make_unique<CallExpr>(std::move(callee), std::move(args), ln);
        }
    }

    return expr;
}

ExprPtr Parser::parse_primary() {
    auto& tok = peek();

    if (check(TokenType::INTEGER)) {
        advance();
        int64_t val = std::stoll(tok.value);
        return std::make_unique<IntLiteral>(val, tok.line);
    }

    if (check(TokenType::FLOAT)) {
        advance();
        double val = std::stod(tok.value);
        return std::make_unique<FloatLiteral>(val, tok.line);
    }

    if (check(TokenType::STRING)) {
        advance();
        return std::make_unique<StringLiteral>(tok.value, tok.line);
    }

    if (check(TokenType::TRUE)) {
        advance();
        return std::make_unique<BoolLiteral>(true, tok.line);
    }

    if (check(TokenType::FALSE)) {
        advance();
        return std::make_unique<BoolLiteral>(false, tok.line);
    }

    if (check(TokenType::NONE)) {
        advance();
        return std::make_unique<NoneLiteral>(tok.line);
    }

    if (check(TokenType::IDENTIFIER) || check(TokenType::RANGE) || check(TokenType::PRINT)) {
        advance();
        return std::make_unique<Identifier>(tok.value, tok.line);
    }

    if (check(TokenType::LPAREN)) {
        advance(); // consume '('
        auto expr = parse_expression();
        expect(TokenType::RPAREN, "expected ')'");
        return expr;
    }

    throw make_error("expected expression");
}

} // namespace mimo
