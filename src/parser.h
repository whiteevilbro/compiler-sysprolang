#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

#include <stdint.h>

typedef struct {
  int64_t value;
} IntLiteralData;

typedef struct {
  bool value;
} BoolLiteralData;

typedef struct {
  enum UnaryOperatorKind : unsigned char {
    UOK_NEG,
    UOK_NOT,
  } kind;
} UnaryOperatorData;

typedef struct {
  enum BinaryOperatorKind : unsigned char {
    BOK_ADD,
    BOK_SUBTRACT,
    BOK_MULTIPLY,
    BOK_DIVIDE,

    BOK_AND,
    BOK_OR,

    BOK_EQ,
    BOK_NE,
    BOK_LT,
    BOK_GT,
    BOK_LE,
    BOK_GE,

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
  const char* filename;
} ProgramData;

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
    NK_IF,
    NK_WHILE,
    NK_BREAK,
    NK_CONTINUE,
    NK_BLOCK, //? is it really needed? cant i just chain statements?

    NK_INT_LITERAL,
    NK_BOOL_LITERAL,

    NK_IDENTIFIER,
    NK_BINARY_OP,
    NK_UNARY_OP,
  } kind;

  size_t offset;

  struct ASTNode* next;
  struct ASTNode* children;

  union {
    IntLiteralData asIntLiteral;
    BoolLiteralData asBoolLiteral;
    UnaryOperatorData asUnaryOperator;
    BinaryOperatorData asBinaryOperator;
    DeclarationData asDeclaration;
    IdentifierData asIndentifier;
    ErrorData asError;
    ProgramData asProgram;
  } data;

} ASTNode;

int parse(TokenList*, ASTNode**, const char*);

#endif
