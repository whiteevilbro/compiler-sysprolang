#ifndef CODEGEN_H
#define CODEGEN_H

#include "parser.h"

#include <stdio.h>

int generate_llvm_ir(FILE*, ASTNode*);

#endif
