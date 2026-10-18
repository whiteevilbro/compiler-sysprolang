#ifndef LEXER_H
#define LEXER_H

#include "vector/vector.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
  enum TokenKind : unsigned char {
    TK_NIL = 0,
    TK_EOF,
    TK_EOS,
    TK_ERROR,

    TK_IDENTIFIER,
    TK_VAL,
    TK_VAR,
    TK_RETURN,

    TK_RIGHT_PARENTHESIS,
    TK_LEFT_PARENTHESIS,

    TK_OP_PLUS,
    TK_OP_MINUS,
    TK_OP_ASTERISK,
    TK_OP_SLASH,
    TK_OP_ASSIGN,

    TK_INT_LITERAL,
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
