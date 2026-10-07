#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

#include <stdint.h>

typedef struct {
  int64_t value;
} IntLiteralData;

typedef struct {
  enum BinaryOperatorKind : unsigned char {
    BOK_ADD,
    BOK_SUBTRACT,
    BOK_MULTIPLY,
    BOK_DIVIDE,

    BOK_ASSIGN,
  } kind;
} BinaryOperatorData;

typedef struct {
  enum DeclarationType : unsigned char {
    DT_VAL,
    DT_VAR,
  } type;
} DeclarationData;

typedef struct {
  //? could I use some kind of unique id here for better memory usage
  //? ofc, it shoud be generated in lexer, but question still stands
  char* name;
} IdentifierData;

typedef struct {
  char placeholder;
} ErrorData;

typedef struct ASTNode {
  enum NodeKind : unsigned char {
    NK_NIL = 0,
    NK_ERROR,
    NK_PROGRAM,
    NK_RETURN,
    NK_DECLARE,
    // NK_ASSIGN, //? do I really need this one? Assignment is a binary operator, though
    NK_INT_LITERAL,
    NK_IDENTIFIER,
    NK_BINARY_OP,
    NK_UNARY,
  } kind;

  size_t offset;

  struct ASTNode* next;
  struct ASTNode* children;

  union {
    IntLiteralData asIntLiteral;
    BinaryOperatorData asBinaryOperator;
    DeclarationData asDeclaration;
    IdentifierData asIndentifier;
    ErrorData asError;
  } data;

} ASTNode;

int parse(TokenList*, ASTNode**);

#endif
