#include "lexer.h"

#include "memory/managment.h"
#include "vector/vector.h"

#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define current_source(lex) ((lex)->source)

#define peek(src) (src)[0]
#define peekOne(src) (src)[1]

#define advance(src) (src)++
#define advanceTwo(src) (src) += 2

#define KEYWORD(keyword, token) {(keyword), sizeof(keyword) - sizeof(char), (token)}

// static inline int next_token(Lexer*, const Token**);
static inline int read_token(Lexer*);
static inline int skip_insignificant(Lexer*, size_t*);
static inline int makeOperatorToken(Lexer*);
static inline int makeNubmerToken(Lexer*);
static inline int makeIdentifierToken(Lexer*);

VecDef(char) String;

//todo: should be sorted by size then lexicographically somewhere in the future (maybe)
static struct {
  const char* word;
  size_t size;
  enum TokenKind kind;
} keywords[] = {
    KEYWORD("if", TK_IF),
    KEYWORD("val", TK_VAL),
    KEYWORD("var", TK_VAR),
    KEYWORD("else", TK_ELSE),
    KEYWORD("true", TK_TRUE),
    KEYWORD("false", TK_FALSE),
    KEYWORD("while", TK_WHILE),
    KEYWORD("break", TK_BREAK),
    KEYWORD("return", TK_RETURN),
    KEYWORD("continue", TK_CONTINUE),
};

void init_lexer(Lexer* lexer, const char* source, Logger* logger, bool save_tokens, const char* source_name) {
  *lexer = (Lexer) {
      .source        = source,
      .offset        = 0,
      .current_token = {.kind = TK_NIL, .offset = ~0},
      .tokens        = {},
      .logger        = logger,
      .flags         = {
                  .save_tokens = save_tokens,
      },
      .source_meta = {
          .name     = source_name,
          .newlines = {},
      }};
}

int get_token(Lexer* lexer, Token* token) {
  peek_token(lexer, token);
  lexer->flags.current_token_read_out = true;
  return lexer->current_status;
}

void unget_token(Lexer* lexer) {
  lexer->flags.current_token_read_out = false;
}

int peek_token(Lexer* lexer, Token* token) {
  if (lexer->current_token.kind == TK_NIL || lexer->flags.current_token_read_out) {
    advance_token(lexer);
    lexer->flags.current_token_read_out = false;
  }
  *token = lexer->current_token;
  return lexer->current_status;
}

int advance_token(Lexer* lexer) {
  if (lexer->current_token.kind == TK_EOF)
    return lexer->current_status;
  lexer->current_status |= read_token(lexer);
  if (lexer->flags.save_tokens) {
    vecPush(&lexer->tokens, lexer->current_token);
  }
  return lexer->current_status;
}

static inline int read_token(Lexer* lexer) {
  Token* token = &lexer->current_token;
  int status   = 0;

  size_t err;
  if (skip_insignificant(lexer, &err)) {
    lexer->current_token = (Token) {.kind = TK_ERROR, .offset = err};
    return 1;
  }
  const char* src = current_source(lexer);
  char c          = peek(src);

  *token = (Token) {.offset = lexer->offset};

  switch (c) {
    case '+':
    case '-':
    case '*':
    case '/':
    case '=':
    case '<':
    case '>':
    case '!':
    case '&':
    case '|':
      return makeOperatorToken(lexer);

    case '(':
      token->kind = TK_LEFT_PARENTHESIS;
      advance(src);
      break;
    case ')':
      token->kind = TK_RIGHT_PARENTHESIS;
      advance(src);
      break;

    case '{':
      token->kind = TK_BLOCK_START;
      advance(src);
      break;
    case '}':
      token->kind = TK_BLOCK_END;
      advance(src);
      break;

    case '\0':
      token->kind = TK_EOF;
      break;
    case ';':
      token->kind = TK_EOS;
      advance(src);
      break;

    default:
      if (isdigit(c)) {
        return makeNubmerToken(lexer);
      }
      if (isalpha(c)) {
        return makeIdentifierToken(lexer);
      }
      //todo: log unexpected character error
      token->kind = TK_ERROR;
      advance(src);
      status |= 1;
      break;
  }

  lexer->offset += src - current_source(lexer);
  lexer->source = src;
  return status;
}

// static inline const char* skip_to(Lexer* lexer, const char target) {
//   const char* src = current_source(lexer);

//   char c;
//   while ((c = peek(src)) && c != target) {
//     if (c == '\n') {
//       size_t offset = lexer->offset + (src - current_source(lexer));
//       vecPush(&lexer->source_meta.newlines, offset);
//     }
//     advance(src);
//   } //todo IN NEED OF REWRITE (comments skip)
//   return src;
// }

// skips any whitespaces and comments
static inline int skip_insignificant(Lexer* lexer, size_t* errorneous_offset) {
  __label__ end;
  const char* src = current_source(lexer);
  int status      = 0;

  enum {
    NORMAL,
    LINE_COMMENT,
    BLOCK_COMMENT,
  } mode = NORMAL;

  char c;
  size_t block_start_offset = (size_t) ~0;
  while ((c = peek(src))) {
    /*
    if (isspace(c)) {
      advance(src);
      if (c == '\n') {
        size_t offset = lexer->offset + (src - current_source(lexer));
        vecPush(&lexer->source_meta.newlines, offset);
      }
    } else if (c == '/') {
      char cc = peekOne(src);
      if (!cc)
        break;

      if (cc == '/') {
        // advanceTwo(src);
        src = skip_to(src, '\n');
        if (*src == '\n') { //! check
          size_t offset = lexer->offset + (src - current_source(lexer));
          vecPush(&lexer->source_meta.newlines, offset);
        }
        advance(src);
      } else if (cc == '*') {
        const char* block_comment_start = src;
        advanceTwo(src);
        do {
          src = skip_to(lexer, '*');
          if (!peek(src)) {
            status             = 1;
            *errorneous_offset = block_comment_start - lexer->source + lexer->offset;
            break;
          }
          advance(src);
          if (!peek(src)) {
            status             = 1;
            *errorneous_offset = block_comment_start - lexer->source + lexer->offset;
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
    */
    if (c == '\n') {
      size_t offset = lexer->offset + (src - current_source(lexer));
      vecPush(&lexer->source_meta.newlines, offset);
    }
    switch (mode) {
      case NORMAL:
        if (isspace(c)) {
          advance(src);
          // if (c == '\n') {
          //   size_t offset = lexer->offset + (src - current_source(lexer));
          //   vecPush(&lexer->source_meta.newlines, offset);
          // }
          break;
        } else if (c == '/') {
          char cc = peekOne(src);
          if (cc == '/') {
            mode = LINE_COMMENT;
            advanceTwo(src);
            break;
          } else if (cc == '*') {
            mode               = BLOCK_COMMENT;
            block_start_offset = lexer->offset + (src - current_source(lexer));
            advanceTwo(src);
            break;
          }
        }
        goto end;

      case LINE_COMMENT:
        if (c == '\n') {
          // size_t offset = lexer->offset + (src - current_source(lexer));
          // vecPush(&lexer->source_meta.newlines, offset);
          mode = NORMAL;
        }
        advance(src);
        break;

      case BLOCK_COMMENT:
        if (c == '*' && peekOne(src) == '/') {
          mode = NORMAL;
          advanceTwo(src);
        } else {
          advance(src);
        }
        break;
    }
  }

end:

  if (mode == BLOCK_COMMENT) {
    assert(block_start_offset != (size_t) ~0);
    *errorneous_offset = block_start_offset;
    status |= 1;
  }
  lexer->offset += src - current_source(lexer);
  lexer->source = src;
  return status;
}

static inline int makeOperatorToken(Lexer* lexer) {
  const char* src = current_source(lexer);

  switch (*src) {
    // arithmetic
    case '+':
      lexer->current_token.kind = TK_OP_PLUS;
      break;
    case '-':
      lexer->current_token.kind = TK_OP_MINUS;
      break;
    case '*':
      lexer->current_token.kind = TK_OP_ASTERISK;
      break;
    case '/':
      lexer->current_token.kind = TK_OP_SLASH;
      break;

    // comparisons and assignment
    case '=':
      if (peekOne(src) == '=') {
        lexer->current_token.kind = TK_OP_EQ;
        advance(src);
        break;
      }
      lexer->current_token.kind = TK_OP_ASSIGN;
      break;
    case '<':
      if (peekOne(src) == '=') {
        lexer->current_token.kind = TK_OP_LE;
        advance(src);
        break;
      }
      lexer->current_token.kind = TK_OP_LT;
      break;
    case '>':
      if (peekOne(src) == '=') {
        lexer->current_token.kind = TK_OP_GE;
        advance(src);
        break;
      }
      lexer->current_token.kind = TK_OP_GT;
      break;
    case '!':
      if (peekOne(src) == '=') {
        lexer->current_token.kind = TK_OP_NE;
        advance(src);
        break;
      }
      lexer->current_token.kind = TK_OP_NOT;
      break;

    // logic
    case '&':
      if (peekOne(src) == '&') {
        lexer->current_token.kind = TK_OP_AND;
        advance(src);
        break;
      }
      lexer->current_token.kind = TK_ERROR;
      break;
    case '|':
      if (peekOne(src) == '|') {
        lexer->current_token.kind = TK_OP_OR;
        advance(src);
        break;
      }
      lexer->current_token.kind = TK_ERROR;
      break;

    default:
      lexer->current_token.kind = TK_ERROR;
  }
  advance(src);
  lexer->offset += src - current_source(lexer);
  lexer->source = src;
  return lexer->current_token.kind == TK_ERROR;
}

static inline int makeNubmerToken(Lexer* lexer) {
  const char* src = current_source(lexer);
  Token* token    = &lexer->current_token;

  assert(isdigit(peek(src)));

  // zero handling
  //? potential non-decimal literals?
  if (peek(src) == '0') {
    if (isalnum(peekOne(src))) { // leading zero
      advanceTwo(src);
      while (isalnum(peek(src))) {
        advance(src);
      }
      lexer->offset += src - current_source(lexer);
      lexer->source             = src;
      lexer->current_token.kind = TK_ERROR;
      return 1;
    } else {
      advance(src);
      lexer->offset += src - current_source(lexer);
      lexer->source     = src;
      token->data.value = 0;
      token->kind       = TK_INT_LITERAL;
      return 0;
    }
  }

  char c;
  int64_t number = 0;
  while (isdigit(c = peek(src))) {
    char d = c - '0';                    // not locale-frienly
    if ((INT64_MAX - d) / 10 < number) { // overflow protection
      do {
        advance(src);
      } while (isdigit(peek(src)));
      lexer->offset += src - current_source(lexer);
      lexer->source             = src; //todo error: literal too big
      lexer->current_token.kind = TK_ERROR;
      return 1;
    }
    number *= 10;
    number += d;
    advance(src);
  }
  lexer->offset += src - current_source(lexer);
  lexer->source             = src;
  lexer->current_token.kind = TK_INT_LITERAL;
  token->data.value         = number;
  return 0;
}

static inline int makeIdentifierToken(Lexer* lexer) {
  const char* src = current_source(lexer);

  String string = {};
  char c;
  while (isalnum(c = peek(src))) {
    advance(src);
    vecPush(&string, c);
  }

  char* name  = vecGetPtr(&string, 0);
  size_t size = vecSize(&string);
  char* str   = smalloc((size + 1) * sizeof(char));
  memcpy(str, name, size);
  str[size] = '\0';

  enum TokenKind kind = TK_IDENTIFIER;

  for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
    if (size != keywords[i].size)
      continue;
    if (!strcmp(str, keywords[i].word)) {
      kind = keywords[i].kind;
      free(str);
    }
  }

  Token* token = &lexer->current_token;

  lexer->offset += src - current_source(lexer);
  lexer->source = src;
  if (kind == TK_IDENTIFIER)
    token->data.str = str;
  token->kind = kind;
  return 0;
}
