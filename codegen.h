// converts the AST into ARM64 assembly for macOS

#ifndef CODEGEN_H
#define CODEGEN_H

#include<stdio.h>
#include "parser.h"

// generates assembly code from the AST and writes it to the output stream
void generate(const Node *program,FILE *out);

#endif