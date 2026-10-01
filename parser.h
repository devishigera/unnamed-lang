// parser.h defines the Abstract Syntax Tree (AST) and declares the parser interface
// the parser converts the flat list of tokens into a tree that represents the structure of the source program

#ifndef PARSER_H 
#define PARSER_H 

#include "lexer.h"

// every possible type of node in our AST
typedef enum{

    // expression nodes
    NODE_INT_LIT, 
    NODE_VAR,     
    NODE_NEGATE,  
    NODE_BINARY,  

    // statement nodes
    NODE_DECL,     
    NODE_ASSIGN,   
    NODE_EXIT,     
    NODE_WRITE,   
    NODE_IF,      
    NODE_WHILE,  
    NODE_BLOCK   
} NodeType;

// each node represents one element of the AST
// fields are used according to the node type
// unused fields remain NULL or zero
typedef struct Node Node;

struct Node{
    NodeType type;    
    int line;         
    long value;     
    const char* name; 
    TokenType op;    
    Node* left;
    Node* right;
    Node* cond;      
    Node* then_branch;
    Node* else_branch;

    Node** stmts;
    size_t stmt_count;   
    size_t stmt_capacity; 
};

// parses a token list and returns the AST root
Node* parse(const TokenList* tokens);

// prints the AST with identation for debugging
void print_ast(const Node *node,int depth);

#endif 