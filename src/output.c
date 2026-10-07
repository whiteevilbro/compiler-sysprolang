#include "output.h"

#include "input.h"
#include "lexer.h"
#include "parser.h"
#include "vector/vector.h"

#include <stdio.h>

static const char* token_string[] = {
    [TK_NIL]   = "NIL",
    [TK_EOF]   = "EOF",
    [TK_EOS]   = "SEMI",
    [TK_ERROR] = "ERROR",

    [TK_IDENTIFIER] = "IDENT",
    [TK_VAL]        = "VAL",
    [TK_VAR]        = "VAR",
    [TK_RETURN]     = "RETURN",

    [TK_RIGHT_PARENTHESIS] = "RPAREN",
    [TK_LEFT_PARENTHESIS]  = "LPAREN",

    [TK_OP_PLUS]     = "PLUS",
    [TK_OP_MINUS]    = "MINUS",
    [TK_OP_ASTERISK] = "MULT",
    [TK_OP_SLASH]    = "DIV",
    [TK_OP_ASSIGN]   = "ASSIGN",

    [TK_INT_LITERAL] = "INT",
};

static const char* ast_string[] = {
    [NK_PROGRAM]     = "Program",
    [NK_ERROR]       = "Error",
    [NK_RETURN]      = "Return",
    [NK_DECLARE]     = "Declare",
    [NK_INT_LITERAL] = "IntLiteral",
    [NK_IDENTIFIER]  = "Ident",
    [NK_BINARY_OP]   = "BinOp",
    [NK_UNARY]       = "Unary",
};

size_t* binsearch(size_t* l, size_t* r, size_t v) {
  while (r - l > 1) {
    size_t* m = l + (r - l + 1) / 2;
    if (*m <= v)
      l = m;
    else
      r = m;
  }
  return l;
}

size_t binary_search(SizeVec* vec, size_t val) {
  size_t* l  = binsearch(vec->inner.data, ((size_t*) vec->inner.data) + vec->inner.len, val);
  size_t idx = l - (size_t*) vec->inner.data;
  return idx;
}

void output_tokens(const char* output_file, TokenList tokens, SizeVec* newlineVec) {
  FILE* output = fopen(output_file, "w");
  fputc('[', output);

  size_t len            = tokens.inner.len;
  size_t nlvi           = 0;
  size_t prev_offset    = 0;
  size_t current_offset = *vecGetPtr(newlineVec, nlvi);

  unsigned int line = 0, column = 0;
  for (size_t i = 0; i < len; i++) {
    Token* token = vecGetPtr(&tokens, i);
    fputs("{\"kind\":\"", output);
    fputs(token_string[token->kind], output);

    fputs("\",\"line\":", output);
    while (current_offset <= token->offset) {
      nlvi++;
      prev_offset    = current_offset;
      current_offset = *vecGetPtr(newlineVec, nlvi);
    }
    line   = nlvi;
    column = token->offset - prev_offset;

    fprintf(output, "%d", line);
    fputs(",\"column\":", output);
    fprintf(output, "%d", column + 1);
    fputc('}', output);
    if (i + 1 != len)
      fputc(',', output);
  }

  fputc(']', output);
  fflush(output);
  fclose(output);
}

void output_ast_r(FILE* output, ASTNode* ast, SizeVec* newlineVec) {
  fputc('{', output);
  fprintf(output, "\"kind\":\"%s\",", ast->kind == NK_BINARY_OP && ast->data.asBinaryOperator.kind == BOK_ASSIGN ? "Assign" : ast_string[ast->kind]);

  size_t idx          = binary_search(newlineVec, ast->offset);
  unsigned int line   = idx + 1;
  unsigned int column = ast->offset - *vecGetPtr(newlineVec, idx) + 1;
  fprintf(output, "\"line\":%u,\"column\":%u,", line, column);

  fputs("\"elems\":[", output);
  if (ast->children)
    output_ast_r(output, ast->children, newlineVec);
  fputs("]}", output);
  if (ast->next) {
    fputc(',', output);
    output_ast_r(output, ast->next, newlineVec);
  }
}

void output_ast(const char* output_file, ASTNode* node, SizeVec* newlines) {
  FILE* out_ast = fopen(output_file, "w");
  if (out_ast) {
    output_ast_r(out_ast, node, newlines);
  }
  fclose(out_ast);
}
