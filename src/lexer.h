#ifndef LEXER_H
#define LEXER_H

#include "vector/vector.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
  enum Kind : unsigned char {
    NIL = 0,
    T_EOF,
    EOS,
    ERROR,

    IDENTIFIER,
    VAL,
    VAR,
    RETURN,

    RIGHT_PARENTHESIS,
    LEFT_PARENTHESIS,

    OP_PLUS,
    OP_MINUS,
    OP_ASTERISK,
    OP_SLASH,
    OP_ASSIGN,

    INT_LITERAL,
  } kind;

  // By Odin's beard, WHY would every token want to know its line and column;
  // its another field to keep track of in EVERY lexer function
  // its really stupid
  // size_t line;
  // size_t column;
  size_t offset;

  union {
    int64_t value;
    void* str;
  } data;
} Token;

VecDef(Token) TokenList;

int tokenize(const char* src, TokenList* const tokens);

#endif
