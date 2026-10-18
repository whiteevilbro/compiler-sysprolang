#include "lexer.h"

#include "memory/managment.h"

#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define peek(src) (src)[0]
#define peekOne(src) (src)[1]
#define peekTwo(src) (src)[2]

#define advance(src) (src)++
#define advanceTwo(src) (src) += 2

Token next_token(const char**);
static inline int skip_insignificant(const char**, size_t*);
static inline enum TokenKind makeOperatorToken(const char** srcp);
static inline enum TokenKind makeNubmerToken(const char** srcp, Token* tokenp);
static inline enum TokenKind makeIdentifierToken(const char** srcp, Token* token);

int tokenize(const char* src, TokenList* const tokens) {
  const char* const start = src;
  size_t global_offset    = 0;
  Token token;
  int status = 0;
  while (true) {
    token = next_token(&src);
    token.offset += global_offset;
    global_offset = src - start;
    vecPush(tokens, token);
    if (token.kind == TK_ERROR)
      status = 1;
    if (token.kind == TK_EOF)
      break;
  }
  return status;
}

Token next_token(const char** srcp) {
  const char* src = *srcp;
  size_t local_offset;
  if (skip_insignificant(&src, &local_offset)) {
    *srcp = src;
    return (Token) {.kind = TK_ERROR, .offset = local_offset};
  }
  local_offset = src - *srcp;
  char c       = peek(src);

  Token token = {.offset = local_offset};

  switch (c) {
    case '+':
    case '-':
    case '*':
    case '/':
    case '=':
      token.kind = makeOperatorToken(&src);
      break;

    case '(':
      token.kind = TK_LEFT_PARENTHESIS;
      advance(src);
      break;
    case ')':
      token.kind = TK_RIGHT_PARENTHESIS;
      advance(src);
      break;

    case '\0':
      token.kind = TK_EOF;
      break;
    case ';':
      token.kind = TK_EOS;
      advance(src);
      break;
    default:
      if (isdigit(c)) {
        token.kind = makeNubmerToken(&src, &token);
        break;
      }
      if (isalpha(c)) {
        token.kind = makeIdentifierToken(&src, &token);
        break;
      }
      token.kind = TK_ERROR;
      advance(src);
      break;
  }

  *srcp = src;
  return token;
}

static inline const char* skip_to(const char* src, const char target) {
  while (peek(src) && peek(src) != target)
    advance(src);
  return src;
}

// skips any whitespaces and comments
static inline int skip_insignificant(const char** srcp, size_t* offset) {
  const char* src = *srcp;
  int status      = 0;


  while (true) {
    char c = peek(src);
    if (!c)
      break;
    if (isspace(c))
      advance(src);
    else if (c == '/') {
      char cc = peekOne(src);
      if (!cc)
        break;

      if (cc == '/') {
        advanceTwo(src);
        src = skip_to(src, '\n');
        advance(src);
      } else if (cc == '*') {
        const char* block_comment_start = src;
        advanceTwo(src);
        do {
          src = skip_to(src, '*');
          if (!peek(src)) {
            status  = 1;
            *offset = block_comment_start - *srcp;
            break;
          }
          advance(src);
          if (!peek(src)) {
            status  = 1;
            *offset = block_comment_start - *srcp;
            break;
          }
          if (peek(src) == '/') {
            advance(src);
            break;
          }
        } while (true);
      } else {
        break;
      }
    } else {
      break;
    }
  }
  *srcp = src;
  return status;
}

static inline enum TokenKind makeOperatorToken(const char** srcp) {
  const char* src = *srcp;
  advance(*srcp);
  switch (*src) {
    case '+':
      return TK_OP_PLUS;
    case '-':
      return TK_OP_MINUS;
    case '*':
      return TK_OP_ASTERISK;
    case '/':
      return TK_OP_SLASH;
    case '=':
      return TK_OP_ASSIGN;
    default:
      return TK_ERROR;
  }
}

static inline enum TokenKind makeNubmerToken(const char** srcp, Token* token) {
  const char* src = *srcp;

  assert(isdigit(peek(src)));

  if (peek(src) == '0') {
    if (isalnum(peekOne(src))) { // leading zero
      advanceTwo(src);
      while (isalnum(peek(src))) {
        advance(src);
      }
      *srcp = src;
      return TK_ERROR;
    } else {
      advance(src);
      *srcp             = src;
      token->data.value = 0;
      return TK_INT_LITERAL;
    }
  }
  char c;
  int64_t number = 0;
  while (isdigit(c = peek(src))) {
    char d = c - '0';                    // not locale-frienly, but at this point i dont care
    if ((INT64_MAX - d) / 10 < number) { // overflow protection
      do {
        advance(src);
      } while (isdigit(peek(src)));
      *srcp = src;
      return TK_ERROR;
    }
    number *= 10;
    number += d;
    advance(src);
  }
  token->data.value = number;
  *srcp             = src;
  return TK_INT_LITERAL;
}

static inline enum TokenKind makeIdentifierToken(const char** srcp, Token* token) {
  const char* src = *srcp;
  size_t size     = 1;
  while (isalnum(peek(src))) {
    advance(src);
    size++;
  }
  char* str = smalloc(size);
  memcpy(str, *srcp, size);
  str[--size] = '\0';

  enum TokenKind kind = TK_NIL;
  if (size == 3) {
    if (!strcmp(str, "val")) {
      kind = TK_VAL;
      free(str);
    } else if (!strcmp(str, "var")) {
      kind = TK_VAR;
      free(str);
    }
  } else if (size == 6) {
    if (!strcmp(str, "return")) {
      kind = TK_RETURN;
      free(str);
    }
  }
  *srcp = src;
  if (kind == TK_NIL) {
    kind            = TK_IDENTIFIER;
    token->data.str = str;
  }
  return kind;
}
