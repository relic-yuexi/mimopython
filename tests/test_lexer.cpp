/**
 * @file test_lexer.cpp
 * @brief Unit tests for the Lexer.
 */
#include <gtest/gtest.h>
#include "lexer.h"

using namespace mimo;

class LexerTest : public ::testing::Test {
protected:
    std::vector<Token> tokenize(const std::string& src) {
        Lexer lexer(src);
        return lexer.tokenize();
    }

    std::vector<TokenType> types(const std::string& src) {
        auto tokens = tokenize(src);
        std::vector<TokenType> result;
        for (auto& t : tokens) result.push_back(t.type);
        return result;
    }
};

TEST_F(LexerTest, EmptyInput) {
    auto toks = tokenize("");
    ASSERT_EQ(toks.size(), 1u);
    EXPECT_EQ(toks[0].type, TokenType::ENDOFFILE);
}

TEST_F(LexerTest, SimpleIdentifier) {
    auto toks = tokenize("hello\n");
    ASSERT_GE(toks.size(), 3u);
    EXPECT_EQ(toks[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(toks[0].value, "hello");
}

TEST_F(LexerTest, IntegerLiteral) {
    auto toks = tokenize("42\n");
    ASSERT_GE(toks.size(), 3u);
    EXPECT_EQ(toks[0].type, TokenType::INTEGER);
    EXPECT_EQ(toks[0].value, "42");
}

TEST_F(LexerTest, FloatLiteral) {
    auto toks = tokenize("3.14\n");
    ASSERT_GE(toks.size(), 3u);
    EXPECT_EQ(toks[0].type, TokenType::FLOAT);
    EXPECT_EQ(toks[0].value, "3.14");
}

TEST_F(LexerTest, StringLiteral) {
    auto toks = tokenize("\"hello world\"\n");
    ASSERT_GE(toks.size(), 3u);
    EXPECT_EQ(toks[0].type, TokenType::STRING);
    EXPECT_EQ(toks[0].value, "hello world");
}

TEST_F(LexerTest, SingleQuoteString) {
    auto toks = tokenize("'test'\n");
    ASSERT_GE(toks.size(), 3u);
    EXPECT_EQ(toks[0].type, TokenType::STRING);
    EXPECT_EQ(toks[0].value, "test");
}

TEST_F(LexerTest, Keywords) {
    auto toks = tokenize("def return if elif else while for in and or not True False None break continue pass\n");
    std::vector<TokenType> expected = {
        TokenType::DEF, TokenType::RETURN, TokenType::IF, TokenType::ELIF,
        TokenType::ELSE, TokenType::WHILE, TokenType::FOR, TokenType::IN,
        TokenType::AND, TokenType::OR, TokenType::NOT, TokenType::TRUE,
        TokenType::FALSE, TokenType::NONE, TokenType::BREAK, TokenType::CONTINUE,
        TokenType::PASS, TokenType::NEWLINE, TokenType::ENDOFFILE
    };
    ASSERT_EQ(toks.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(toks[i].type, expected[i]) << "Token " << i;
    }
}

TEST_F(LexerTest, Operators) {
    auto toks = tokenize("+ - * / // % == != < > <= >= =\n");
    std::vector<TokenType> expected = {
        TokenType::PLUS, TokenType::MINUS, TokenType::STAR, TokenType::SLASH,
        TokenType::SLASHSLASH, TokenType::PERCENT, TokenType::EQ, TokenType::NEQ,
        TokenType::LT, TokenType::GT, TokenType::LTE, TokenType::GTE,
        TokenType::ASSIGN, TokenType::NEWLINE, TokenType::ENDOFFILE
    };
    ASSERT_EQ(toks.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(toks[i].type, expected[i]) << "Token " << i;
    }
}

TEST_F(LexerTest, ParenthesesAndComma) {
    auto toks = tokenize("f(a, b)\n");
    std::vector<TokenType> expected = {
        TokenType::IDENTIFIER, TokenType::LPAREN, TokenType::IDENTIFIER,
        TokenType::COMMA, TokenType::IDENTIFIER, TokenType::RPAREN,
        TokenType::NEWLINE, TokenType::ENDOFFILE
    };
    ASSERT_EQ(toks.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(toks[i].type, expected[i]) << "Token " << i;
    }
}

TEST_F(LexerTest, IndentDedent) {
    std::string src = "if True:\n    x = 1\n    y = 2\n";
    auto toks = tokenize(src);
    // Should have: IF TRUE COLON NEWLINE INDENT IDENT ASSIGN INT NEWLINE IDENT ASSIGN INT NEWLINE DEDENT EOF
    bool has_indent = false, has_dedent = false;
    for (auto& t : toks) {
        if (t.type == TokenType::INDENT) has_indent = true;
        if (t.type == TokenType::DEDENT) has_dedent = true;
    }
    EXPECT_TRUE(has_indent);
    EXPECT_TRUE(has_dedent);
}

TEST_F(LexerTest, NestedIndent) {
    std::string src = "if True:\n    if True:\n        x = 1\n";
    auto toks = tokenize(src);
    int indent_count = 0, dedent_count = 0;
    for (auto& t : toks) {
        if (t.type == TokenType::INDENT) indent_count++;
        if (t.type == TokenType::DEDENT) dedent_count++;
    }
    EXPECT_EQ(indent_count, 2);
    EXPECT_EQ(dedent_count, 2);
}

TEST_F(LexerTest, CommentHandling) {
    auto toks = tokenize("# comment\nx = 1\n");
    ASSERT_GE(toks.size(), 3u);
    EXPECT_EQ(toks[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(toks[0].value, "x");
}

TEST_F(LexerTest, UnterminatedString) {
    Lexer lexer("\"unterminated\n");
    auto toks = lexer.tokenize();
    EXPECT_TRUE(lexer.has_errors());
}

TEST_F(LexerTest, IllegalCharacter) {
    Lexer lexer("@\n");
    auto toks = lexer.tokenize();
    EXPECT_TRUE(lexer.has_errors());
}

TEST_F(LexerTest, LineNumbers) {
    auto toks = tokenize("x\ny\n");
    ASSERT_GE(toks.size(), 4u);
    EXPECT_EQ(toks[0].line, 1u); // x
    EXPECT_EQ(toks[2].line, 2u); // y
}

TEST_F(LexerTest, ColonToken) {
    auto toks = tokenize(":\n");
    ASSERT_GE(toks.size(), 3u);
    EXPECT_EQ(toks[0].type, TokenType::COLON);
}

TEST_F(LexerTest, PrintKeyword) {
    auto toks = tokenize("print(x)\n");
    ASSERT_GE(toks.size(), 5u);
    EXPECT_EQ(toks[0].type, TokenType::PRINT);
}

TEST_F(LexerTest, EmptyLines) {
    std::string src = "x = 1\n\n\ny = 2\n";
    auto toks = tokenize(src);
    // Should handle empty lines gracefully
    bool has_x = false, has_y = false;
    for (auto& t : toks) {
        if (t.type == TokenType::IDENTIFIER && t.value == "x") has_x = true;
        if (t.type == TokenType::IDENTIFIER && t.value == "y") has_y = true;
    }
    EXPECT_TRUE(has_x);
    EXPECT_TRUE(has_y);
}

TEST_F(LexerTest, IndentAfterEmptyLine) {
    std::string src = "if True:\n\n    x = 1\n";
    auto toks = tokenize(src);
    bool has_indent = false;
    for (auto& t : toks) {
        if (t.type == TokenType::INDENT) has_indent = true;
    }
    EXPECT_TRUE(has_indent);
}
