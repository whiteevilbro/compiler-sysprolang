#ifndef OUTPUT_H
#define OUTPUT_H

#include "input.h"
#include "lexer.h"
#include "parser.h"

void output_tokens(const char*, TokenList, SizeVec*);
void output_ast(const char*, ASTNode*, SizeVec*);

#endif
