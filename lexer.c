#include "lexer.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// small helpers

// add a token to the token list
// if the array is full, increase it's capacity first
static void push_token(TokenList *list,Token tok){

    // grow array when there is no free space
    if(list->count == list->capacity){

        // start with 64 slots, then double the size whenever needed
        if(list->capacity==0) list->capacity=64;
        else list->capacity=list->capacity*2;

        // resize the memory to hold the new number of tokens
        list->items=realloc(list->items,list->capacity*sizeof(Token));

        // stop if memory allocation failed
        if(list->items==NULL){
            fprintf(stderr,"out of memory\n");
            exit(1);
        }
    }

    // add the new token to the end and update the count
    list->items[list->count++]=tok;
}


// create a toekn and copy its text into separately allocated memory
static Token make_token(const TokenType type,const char *start,size_t len,int line){

    Token t;
    
    // store basic information about token
    t.type=type;
    t.value=0;
    t.line=line;

    // allocate space for the token text + '\0'
    t.text=malloc(len+1);

    // copy the token's characters into the new memory
    memcpy(t.text,start,len);

    // make the copied characters a valid C string
    t.text[len]='\0';

    return t;
}


// decide whether a word is a reserved keyword or a normal identifier
static TokenType keyword_or_ident(const char *word){

    if(strcmp(word,"int")==0) return TOK_INT;

    if(strcmp(word,"exit")==0) return TOK_EXIT;

    if(strcmp(word,"write")==0) return TOK_WRITE;

    if(strcmp(word,"if")==0) return TOK_IF;

    if(strcmp(word,"else")==0) return TOK_ELSE;
    
    if(strcmp(word,"while")==0) return TOK_WHILE;

    return TOK_IDENT;

}


// lex the source code and convert it into a list of tokens
TokenList lex(const char *source){

    // start with an empty token list
    TokenList list={0};

    // p points to the character currently being examined
    const char *p=source;

    // keep track of the current source-code line
    int line=1;

    // process characters until we reach the end of the source string
    while(*p != '\0'){
        //store the current character for easier use below
        char c=*p;

        // newlines: cont them for error messages, then skip
        if(c=='\n'){
            line++;
            p++;
            continue;
        }

        // ignore other whitespaces such as spaces and tabs
        if(isspace((unsigned char)c)){
            p++;
            continue;
        }

        // if we see '//', skip everything until the end of the line
        if(c=='/' && p[1]=='/'){
            while(*p!='\0' && *p!='\n') p++;
            continue;
        }

        // read a sequence of digits as an integer literal
        if(isdigit((unsigned char)c)){
            const char *start=p;
            long value=0;

            // keep reading while the current character is a digit
            while(isdigit((unsigned char)*p)){
                int digit=*p-'0';

                // make sure adding this digit won't overflow long
                if(value>(LONG_MAX-digit)/10){
                    fprintf(stderr,"Lexer error (line %d): number too large\n",line);
                    exit(1);
                }

                // add the new digit to the number
                value=value*10+digit;
                p++;
            }

            // create a token containing the number's text and value
            Token t=make_token(TOK_INT_LIT,start,(size_t)(p-start),line);
            t.value=value;

            push_token(&list,t);
            continue;
        }


        // read a word: either a keyword or an identifier
        if(isalpha((unsigned char)c) || c == '_'){
            const char *start=p;

            // keep reading letters, digits and underscores
            while(isalnum((unsigned char)*p) || *p=='_') p++;

            // initially treat the word as an identifier
            Token t=make_token(TOK_IDENT,start,(size_t)(p-start),line);

            // change the type if the word is a reserved keyword
            t.type=keyword_or_ident(t.text);

            push_token(&list,t);
            continue;
        }


        // check for two-character operators such as ==, !=, <= and >=
        if(p[1]=='='){
            TokenType two=TOK_EOF;

            if(c=='=') two=TOK_EQ;
            if(c=='!') two=TOK_NEQ;
            if(c=='<') two=TOK_LE;
            if(c=='>') two=TOK_GE;

            // if we found a valid two-character operator, create it's token
            if(two!=TOK_EOF){
                push_token(&list,make_token(two,p,2,line));

                // we consumed both characters
                p+=2;
                continue;
            }
        }

        // handle operators and symbols made of a single character
        TokenType type;
        switch(c){
            case '(': type=TOK_LPAREN; break;
            case ')': type=TOK_RPAREN; break;
            case '{': type=TOK_LBRACE; break;
            case '}': type=TOK_RBRACE; break;
            case ';': type=TOK_SEMI; break;
            case '=': type=TOK_ASSIGN; break;
            case '+': type=TOK_PLUS; break;
            case '-': type=TOK_MINUS; break;
            case '*': type=TOK_STAR; break;
            case '/': type=TOK_SLASH; break;
            case '%': type=TOK_PERCENT; break;
            case '<': type=TOK_LT; break;
            case '>': type=TOK_GT; break;

            default:
                fprintf(stderr,"Lexer error (line %d): unexpected character '%c'\n",line,c);
                exit(1);
        }

        // create a token for the single character we just recongnised
        push_token(&list,make_token(type,p,1,line));
        // move past the character we just consumed
        p++;

        
    }
    // always finish with an EOF token so the parser knows where to stop
        push_token(&list,make_token(TOK_EOF,p,0,line));
        return list;
}

// convert a token type into a readable name for debusgging
const char* token_type_name(TokenType type){
    switch(type){
        case TOK_INT_LIT: return "INT_LIT";
        case TOK_IDENT: return "IDENT";
        case TOK_INT: return "INT";
        case TOK_EXIT: return "EXIT";
        case TOK_WRITE: return "WRITE";
        case TOK_IF: return "IF";
        case TOK_ELSE: return "ELSE";
        case TOK_WHILE: return "WHILE";
        case TOK_LPAREN: return "LPAREN";
        case TOK_RPAREN: return "RPAREN";
        case TOK_LBRACE: return "LBRACE";
        case TOK_RBRACE:  return "RBRACE";
        case TOK_SEMI:    return "SEMI";
        case TOK_ASSIGN:  return "ASSIGN";
        case TOK_PLUS:    return "PLUS";
        case TOK_MINUS:   return "MINUS";
        case TOK_STAR:    return "STAR";
        case TOK_SLASH:   return "SLASH";
        case TOK_PERCENT: return "PERCENT";
        case TOK_EQ:      return "EQ";
        case TOK_NEQ:     return "NEQ";
        case TOK_LT:      return "LT";
        case TOK_GT:      return "GT";
        case TOK_LE:      return "LE";
        case TOK_GE:      return "GE";
        case TOK_EOF:     return "EOF";
    }

    return "UNKNOWN";
}

void print_tokens(const TokenList* list){
    for(size_t i=0;i<list->count;i++){
        const Token *t=&list->items[i];

        printf("line %-3d %-8s '%s'\n",
       t->line,
       token_type_name(t->type),
       t->text);
    }
}