#!/bin/bash

gcc main.c lexer.c parser.c codegen.c -o main -Wall -Wextra
