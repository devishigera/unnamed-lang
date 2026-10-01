// this header file defines the token types, token structures, token list and functios used by our lexer.

// the actual function implementations are written in lexer.c

#ifndef LEXER_H
#define LEXER_H

#include<stddef.h>

typedef enum{
    TOK_INT_LIT,   // integer literal
    TOK_IDENT,     // variable or identifier

    // keywords
    TOK_INT,
    TOK_EXIT,
    TOK_WRITE,
    TOK_IF,
    TOK_ELSE,
    TOK_WHILE,

    // parentheses and braces
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,

    // punctuation and operators
    TOK_SEMI,
    TOK_ASSIGN,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_PERCENT,
    TOK_EQ,
    TOK_NEQ,
    TOK_LT,
    TOK_GT,
    TOK_LE,
    TOK_GE,

    // end of input
    TOK_EOF
} TokenType;

// represents a single token produced by the lexer
typedef struct{
    TokenType type;
    char* text;
    long value;
    int line;
} Token;

// dynamic array useed to store the tokens generated during the lexical analysis
typedef struct{
    Token* items;
    size_t count;
    size_t capacity;
} TokenList;

// lexer interface
TokenList lex(const char* source);
const char *token_type_name(TokenType type);
void print_tokens(const TokenList* list);

#endif