// converts the AST into ARM64 assembly for macOS

// the code generator traverses the AST produced by the parser and emits assembly instructions for Apple Silicon

#include "codegen.h"

#include <stdarg.h>
#include<stdlib.h>
#include<string.h>

// maximum number of variables and nested scopes supported
#define MAX_VARS 255
#define MAX_SCOPES 256

// stores information about a declared variable
typedef struct{
    const char* name;
    int slot;
}Variable;

// maintains the state needed during code generation
typedef struct{
    FILE* out;

    Variable vars[MAX_VARS];
    int var_count;

    int scope_start[MAX_SCOPES];
    int scope_depth;

    int label_counter;
} CodeGen;

// writes an indented assembly instruction to the output
static void emit(CodeGen* g,const char* fmt,...){
    va_list args;

    va_start(args,fmt);
    fprintf(g->out,"    ");
    vfprintf(g->out,fmt,args);
    fprintf(g->out,"\n");
    va_end(args);
}

// writes an assembly label without indentation
static void emit_label(CodeGen* g,const char* prefix,int id){
    fprintf(g->out,"%s_%d:\n",prefix,id);
}

// reports a code generation error with its source line
static void gen_error(const Node* n,const char* msg,const char* name){
    fprintf(stderr,"Error (line %d): %s '%s'\n",n->line,msg,name);
    exit(1);
}

// saves the value in x0 on the stack, maintaining 16-byte alignment
static void push_x0(CodeGen* g) {
    emit(g, "str x0, [sp, #-16]!");
}

static void pop_into(CodeGen* g,const char* reg){
    emit(g,"ldr %s, [sp], #16",reg);
}

// loads a 64-bit integer constant into x0 using 16-bit chunks
static void load_int(CodeGen *g,long value){
    unsigned long v=(unsigned long)value;

    emit(g,"movz x0, #%lu",v & 0xFFFF);

    for(int shift=16;shift<64;shift+=16){
        unsigned long chunk=(v>>shift) & 0xFFFF;

        if(chunk!=0){
            emit(g,"movk x0, #%lu, lsl #%d",chunk,shift);
        }
    }
}

static Variable* find_var(CodeGen* g,const char* name){
    for(int i=g->var_count-1;i>=0;i--){
        if(strcmp(g->vars[i].name,name)==0) return &g->vars[i];
    }
    return NULL;
}

static void var_address(CodeGen* g,const Variable* v){
    emit(g,"sub x9, x29, #%d", 16 * (v->slot + 1));
}

static void begin_scope(CodeGen* g){
    if(g->scope_depth==MAX_SCOPES){
        fprintf(stderr,"Error: blocks nested into deeply\n");
        exit(1);
    }
    g->scope_start[g->scope_depth++]=g->var_count;
}

static void end_scope(CodeGen* g){
    int start=g->scope_start[--g->scope_depth];
    int count=g->var_count-start;
    if(count>0){
        emit(g,"add sp, sp, #%d",count+16);
    }
    g->var_count-=count;
    g->scope_depth--;
}

// generates assembly for expressions
// the result of every expression is stored in x0
static void gen_expr(CodeGen* g,const Node* n){
    switch(n->type){

        case NODE_INT_LIT:
        load_int(g,n->value);
        break;

        case NODE_VAR:{
            Variable* v=find_var(g,n->name);

            if(v==NULL) gen_error(n,"undeclared variable",n->name);

            var_address(g,v);
            emit(g,"ldr x0, [x9]");
            break;
        }

        case NODE_NEGATE:
        gen_expr(g,n->left);
        emit(g,"    neg x0,x0\n");
        break;

        case NODE_BINARY:
        gen_expr(g,n->left);
        push_x0(g);
        gen_expr(g,n->right);
        pop_into(g,"x1");

        switch(n->op){
            case TOK_PLUS:
            emit(g,"add x0, x1, x0");
            break;

            case TOK_MINUS:
            emit(g,"sub x0, x1, x0");
            break;

            case TOK_STAR:
            emit(g,"mul x0, x1, x0");
            break;

            case TOK_SLASH:
            emit(g,"sdiv x0, x1, x0");
            break;

            case TOK_PERCENT:
            emit(g,"sdiv x2, x1, x0");
            emit(g,"msub x0, x2, x0, x1");
            break;

            case TOK_EQ:
            emit(g,"cmp x1, x0");
            emit(g,"cset x0, eq");
            break;

            case TOK_NEQ:
            emit(g,"cmp x1, x0");
            emit(g,"cset x0, ne");
            break;

            case TOK_LT:
            emit(g,"cmp x1, x0");
            emit(g,"cset x0, lt");
            break;

            case TOK_GT:
            emit(g,"cmp x1, x0");
            emit(g,"cset x0, gt");
            break;

            case TOK_LE:
            emit(g,"cmp x1, x0");
            emit(g,"cset x0, le");
            break;

            case TOK_GE:
            emit(g,"cmp x1, x0");
            emit(g,"cset x0, ge");
            break;

            default:
            fprintf(stderr,"Internal error: unknown operator\n");
            exit(1);
        }
        break;

        default:
        fprintf(stderr,"Internal error: node is not an expression\n");
        exit(1);

    }
}


static void gen_stmt(CodeGen* g,const Node* n);


// generate assembly for a block of statements
static void gen_block(CodeGen* g,const Node* block){
    begin_scope(g);

    // generate code for each statement int he block
    for(size_t i=0;i<block->stmt_count;i++){
        gen_stmt(g,block->stmts[i]);
    }
    end_scope(g);
}


// 
static void gen_stmt(CodeGen* g,const Node* n){
    switch(n->type){

        case NODE_DECL:{
            // reject duplicate names in the current scope
            int start=g->scope_start[g->scope_depth-1];

            for(int i=start;i<g->var_count;i++){
                if(strcmp(g->vars[i].name,n->name)==0) gen_error(n,"variable already declared in this scope",n->name);
            }

            if(g->var_count==MAX_VARS) gen_error(n,"too many variables at",n->name);

            // evaluating before registering the new name, allowing outer-scope lookup
            fprintf(g->out,"    // int %s = ...\n",n->name);
            gen_expr(g,n->left);
            push_x0(g);

            g->vars[g->var_count].name=n->name;
            g->vars[g->var_count].slot=g->var_count;
            g->var_count++;

            break;
        }
        
        
        case NODE_ASSIGN:{
            // find the variable being assigned
            Variable* v=find_var(g,n->name);

            if(v==NULL) gen_error(n,"assignment to undeclared variable",n->name);

            fprintf(g->out,"    // %s = ...\n",n->name);

            // evaluate the new value, then store it in the variable's stack slot
            gen_expr(g,n->left);
            var_address(g,v);
            emit(g,"str x0, [x9]");

            break;
        }
        
        
        case NODE_EXIT:{
            // evaluate the exit code and pass it to the system
            fprintf(g->out,"    // exit(...)\n");
            gen_expr(g,n->left);
            emit(g,"bl _exit");
            break;
        }


        case NODE_WRITE: {
            // evaluate the value that needs to be printed
            fprintf(g->out, "    // write(...)\n");
            gen_expr(g, n->left);

            // reserve stack space and save the value
            emit(g, "sub sp, sp, #16");
            emit(g, "str x0, [sp]");

            // load the format string and call printf
            emit(g, "adrp x0, l_fmt@PAGE");
            emit(g, "add x0, x0, l_fmt@PAGEOFF");
            emit(g, "bl _printf");

            // restore the stack pointer
            emit(g, "add sp, sp, #16");
            break;
        }


        case NODE_IF: {
            int id = g->label_counter++;

            fprintf(g->out, "    // if\n");

            // evaluate the condition. A zero result means false
            gen_expr(g, n->cond);
            emit(g, "cbz x0, Lelse_%d", id);

            // execute the then-branch
            gen_block(g, n->then_branch);
            emit(g, "b Lendif_%d", id);

            emit_label(g, "Lelse", id);

            // handle else blocks and else-if statements
            if (n->else_branch) {
                if (n->else_branch->type == NODE_BLOCK)
                    gen_block(g, n->else_branch);
                else
                    gen_stmt(g, n->else_branch);
            }

            emit_label(g, "Lendif", id);
            break;
        }


        case NODE_WHILE: {
            int id = g->label_counter++;

            // mark the beginning of the loop
            emit_label(g, "Lwhile", id);

            // exit the loop when the condition becomes false
            gen_expr(g, n->cond);
            emit(g, "cbz x0, Lendwhile_%d", id);

            // generate the loop body and jump back to the condition
            gen_block(g, n->then_branch);
            emit(g, "b Lwhile_%d", id);

            emit_label(g, "Lendwhile", id);
            break;
        }


        case NODE_BLOCK:
            gen_block(g, n);
            break;

        default:
            fprintf(stderr, "Internal error (line %d): not a statement\n", n->line);
            exit(1);
    }
}


// entry point for generating ARM64 assembly from the AST
void generate(const Node *program, FILE *out) {
    CodeGen g;

    // initialize all generator state to zero
    memset(&g, 0, sizeof g);
    g.out = out;

    // prologue: set up the program's entry point and stack frame
    fprintf(out, "// Generated by unnc. Target: ARM64 macOS (Apple Silicon)\n");
    fprintf(out, ".section __TEXT,__text\n");
    fprintf(out, ".globl _main\n");
    fprintf(out, ".p2align 2\n");
    fprintf(out, "_main:\n");

    emit(&g, "stp x29, x30, [sp, #-16]!");
    emit(&g, "mov x29, sp");

    fprintf(out, "\n");

    // generate assembly for the user's program
    gen_block(&g, program);

    // epilogue: return 0 if the program did not call exit()
    fprintf(out, "\n    // end of program: return 0\n");
    emit(&g, "mov x0, #0");
    emit(&g, "mov sp, x29");
    emit(&g, "ldp x29, x30, [sp], #16");
    emit(&g, "ret");

    // store the format string used by printf
    fprintf(out, "\n.section __TEXT,__cstring\n");
    fprintf(out, "l_fmt:\n");
    fprintf(out, "    .asciz \"%%ld\\n\"\n");
}