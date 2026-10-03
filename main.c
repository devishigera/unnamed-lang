#include<stdio.h>
#include<stdlib.h>

#include "lexer.h"
#include "parser.h"
#include "codegen.h"

int main(){
	
	FILE* file=fopen("test.unn","r");
	
	if(file==NULL){
		perror("Error opening file");
		return 1;
	}

	fseek(file,0,SEEK_END);
	long file_size=ftell(file);
	rewind(file);

	char* source=malloc(file_size+1);

	if(source==NULL){
		perror("Memory allocation failed");
		fclose(file);
		return 1;
	}

	fread(source,1,file_size,file);
	source[file_size]='\0';

	fclose(file);

	TokenList tokens=lex(source);

	print_tokens(&tokens);

	Node* ast=parse(&tokens);

	print_ast(ast,0);

	FILE *out = fopen("output.s", "w");

	if (out == NULL) {
		perror("Could not create output.s");
		return 1;
	}

	generate(ast, out);

	fclose(out);

	printf("Assembly generated successfully!\n");

	return 0;
}
