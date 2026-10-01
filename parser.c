#include "parser.h"

#include <stdio.h>
#include <stdlib.h>


// the parser keeps track of the token list and it's current position while parsing
typedef struct{
    const TokenList* tokens;
    size_t pos;
} Parser;


// returns the current token without moving forward
static Token *peek(Parser* p){
    return &p->tokens->items[p->pos];
}


// returns the current token and moves to the next
static Token* advance(Parser* p){
    Token* t=peek(p);

    if(t->type != TOK_EOF){
        p->pos++;
    }

    return t;
}


// checks whether the current token matches the given type
static int check(Parser* p,TokenType type){
    return peek(p)->type==type;
}


// consumes the current token if it matches the given type
static int match(Parser* p,TokenType type){
    if(check(p,type)){
        advance(p);
        return 1;
    }
    return 0;
}


// prints a syntax error with the line number and exits
static void parse_error(const Token* t,const char* expected){
    fprintf(stderr,"Parse error (line %d): expected %s, but found %s'\n",
    t->line, expected,
    t->type==TOK_EOF ? "end of file" : t->text);

    exit(1);
}


// requires a specific token; reports an error if it is missing
static Token *expect(Parser* p,TokenType type,const char* what){
    if(!check(p,type)){
        parse_error(peek(p),what);
    }

    return advance(p);
}


// allocates and initializes a new AST node
static Node* new_node(NodeType type,int line){
    Node* n=calloc(1,sizeof(Node));

    if(n==NULL){
        fprintf(stderr,"Out of memory\n");
        exit(1);
    }

    n->type=type;
    n->line=line;

    return n;
}


// adds a statement to a block, expanding it's array when needed
static void block_add(Node* block,Node* stmt){

    if(block->stmt_count == block->stmt_capacity){
        block->stmt_capacity=block->stmt_capacity?block->stmt_capacity*2:8;

        block->stmts=realloc(
            block->stmts,
            block->stmt_capacity*sizeof(Node *)
        );

        if(block->stmts==NULL){
            fprintf(stderr,"Out of memory\n");
            exit(1);
        }
    }

    block->stmts[block->stmt_count++]=stmt;
}


// creates a binary expression node with two child nodes
static Node* new_binary(TokenType op,Node* left,Node* right,int line){
    Node* n=new_node(NODE_BINARY,line);

    n->op=op;
    n->left=left;
    n->right=right;

    return n;
}


// forward declaration: parse_expression is defined later
static Node *parse_expression(Parser* p);
static Node *parse_statement(Parser *p);


// parses literals, variables, parentheses and unary minus
static Node* parse_factor(Parser* p){
    Token* t=peek(p);

    // integer literal
    if(match(p,TOK_INT_LIT)){
        Node* n=new_node(NODE_INT_LIT,t->line);
        n->value=t->value;
        return n;
    }

    // variable reference
    if(match(p,TOK_IDENT)){
        Node* n=new_node(NODE_VAR,t->line);
        n->name=t->text;
        return n;
    }

    // parenthesized expression
    if(match(p,TOK_LPAREN)){
        Node* n=parse_expression(p);
        expect(p,TOK_RPAREN,"')'");
        return n;
    }

    // unary minus
    if(match(p,TOK_MINUS)){
        Node* n=new_node(NODE_NEGATE,t->line);
        n->left=parse_factor(p);
        return n;
    }

    parse_error(t,"expression");
    return NULL;
}


// parses multiplication, division, and modulo expression
static Node* parse_term(Parser* p){
    Node* left=parse_factor(p);

    while(check(p,TOK_STAR) || check(p,TOK_SLASH) || check(p,TOK_PERCENT)){
        Token* op=advance(p);
        Node* right=parse_factor(p);
        left=new_binary(op->type,left,right,op->line);
    }
    return left;
}


// parses addition and subtration expressions
static Node* parse_additive(Parser* p){
    Node* left=parse_term(p);

    while(check(p,TOK_PLUS) || check(p,TOK_MINUS)){
        Token* op=advance(p);
        Node* right=parse_term(p);

        left=new_binary(op->type,left,right,op->line);
    }

    return left;
}


// parses comparision expressions after evaluating arithmetic expressions
static Node* parse_expression(Parser* p){
    Node* left=parse_additive(p);

    if(check(p,TOK_EQ) || check(p,TOK_NEQ) || check(p,TOK_LT) || check(p,TOK_GT) || check(p,TOK_LE) || check(p,TOK_GE)){
        Token* op=advance(p);
        Node* right=parse_additive(p);

        left=new_binary(op->type,left,right,op->line);
    }
    return left;
}


// parses a block containing zero or more statements
static Node* parse_block(Parser* p){
    Token* start=expect(p,TOK_LBRACE,"'{'");

    Node* block=new_node(NODE_BLOCK,start->line);

    while(!check(p,TOK_RBRACE) && !check(p,TOK_EOF)){
        block_add(block,parse_statement(p));
    }

    expect(p,TOK_RBRACE,"'}'");
    return block;
}


// shared parser for exit(expr); and write(expr);
static Node *parse_call_like(Parser* p,NodeType type){
    Token *kw=advance(p);

    expect(p,TOK_LPAREN,"'('");

    Node* n=new_node(type,kw->line);
    n->left=parse_expression(p);

    expect(p,TOK_RPAREN,"')'");
    expect(p,TOK_SEMI,"';'");

    return n;
}


// parses a statement based on the current token
static Node* parse_statement(Parser* p){
    Token* t=peek(p);

    switch(t->type){
        
        case TOK_INT:{
            advance(p);

            Token* name=expect(p,TOK_IDENT,"a variable name");
            expect(p,TOK_ASSIGN,"'='");

            Node* n=new_node(NODE_DECL,t->line);
            n->name=name->text;
            n->left=parse_expression(p);

            expect(p,TOK_SEMI,"';'");
            return n;
        }

        case TOK_IDENT:{
            advance(p);

            expect(p,TOK_ASSIGN,"'='");

            Node* n=new_node(NODE_ASSIGN,t->line);
            n->name=t->text;
            n->left=parse_expression(p);

            expect(p,TOK_SEMI,"';'");
            return n;
        }

        case TOK_EXIT:
        return parse_call_like(p,NODE_EXIT);

        case TOK_WRITE:
        return parse_call_like(p,NODE_WRITE);

        case TOK_IF:{
            advance(p);

            Node* n=new_node(NODE_IF,t->line);

            expect(p,TOK_LPAREN,"'('");
            n->cond=parse_expression(p);
            expect(p,TOK_RPAREN,"')'");

            n->then_branch=parse_block(p);

            if(match(p,TOK_ELSE)){
                if(check(p,TOK_IF)){
                    n->else_branch=parse_statement(p);
                }else{
                    n->else_branch=parse_block(p);
                }
            }
            return n;
        }

        case TOK_WHILE:{
            advance(p);

            Node* n=new_node(NODE_WHILE,t->line);

            expect(p,TOK_LPAREN,"'('");
            n->cond=parse_expression(p);
            expect(p,TOK_RPAREN,"')'");

            n->then_branch=parse_block(p);

            return n;
        }

        case TOK_LBRACE: 
        return parse_block(p);

        default:
        parse_error(t,"a statement");
        return NULL;
    }
}


// parses all statements and builds the program AST
Node* parse(const TokenList* tokens){
    Parser p={tokens,0};

    Node* program=new_node(NODE_BLOCK,1);

    while(!check(&p,TOK_EOF)){
        block_add(program,parse_statement(&p));
    }
    
    return program;
}


// prints spaces to show the depth of a node int he AST
static void indent(int depth){
    for(int i=0;i<depth;i++){
        printf(" ");
    }
}


// prints the AST recursively, one node at a time
void print_ast(const Node* node,int depth){
    if(node==NULL) return;

    indent(depth);

    switch(node->type){

        case NODE_INT_LIT:
        printf("Int %ld\n",node->value);
        break;

        case NODE_VAR:
        printf("Var %s\n",node->name);
        break;

        case NODE_NEGATE:
        printf("Negate\n");
        print_ast(node->left,depth+1);
        break;

        case NODE_BINARY:
        printf("Binary %s\n",token_type_name(node->op));
        print_ast(node->left,depth+1);
        print_ast(node->right,depth+1);
        break;

        case NODE_DECL:
        printf("Decl %s\n",node->name);
        print_ast(node->left,depth+1);
        break;

        case NODE_ASSIGN:
        printf("Assign %s\n",node->name);
        print_ast(node->left,depth+1);
        break;

        case NODE_EXIT:
        printf("Exit\n");
        print_ast(node->left,depth+1);
        break;

        case NODE_WRITE:
        printf("Write\n");
        print_ast(node->left,depth+1);
        break;

        case NODE_IF:
        printf("If\n");

        indent(depth+1);
        printf("Condition\n");
        print_ast(node->cond,depth+2);

        indent(depth+1);
        printf("Then:\n");
        print_ast(node->then_branch,depth+2);

        if(node->else_branch!=NULL){
            indent(depth+1);
            printf("Else:\n");
            print_ast(node->else_branch,depth+2);
        }
        break;

        case NODE_WHILE:
        printf("While\n");

        indent(depth+1);
        printf("Condition:\n");
        print_ast(node->cond,depth+2);

        indent(depth+1);
        printf("Body:\n");
        print_ast(node->then_branch,depth+2);
        break;

        case NODE_BLOCK:
        printf("Block\n");

        for(size_t i=0;i<node->stmt_count;i++){
            print_ast(node->stmts[i],depth+1);
        }
        break;
    }
}