#include "lexer.h"
#include "memory/managment.h"
#include "parser.h"
#include "vector/vector.h"

#include <stdio.h>
#include <stdlib.h>

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

void output_tokens(const char* output_file, TokenList tokens, const char* source) {
  FILE* output = fopen(output_file, "w");
  fputc('[', output);

  size_t len = tokens.inner.len;
  size_t bi  = 0;

  unsigned int line = 0, column = 0;
  for (size_t i = 0; i < len; i++) {
    Token* token = vecGetPtr(&tokens, i);
    fputs("{\"kind\":\"", output);
    fputs(token_string[token->kind], output);

    fputs("\",\"line\":", output);
    while (bi < token->offset) {
      if (source[bi++] == '\n') {
        line++;
        column = 0;
      } else {
        column++;
      }
    }
    fprintf(output, "%d", line + 1);
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

void output_ast(FILE* output, ASTNode* ast) {
  fputc('{', output);
  fprintf(output, "\"kind\":\"%s\",", ast->kind == NK_BINARY_OP && ast->data.asBinaryOperator.kind == BOK_ASSIGN ? "Assign" : ast_string[ast->kind]);
  fputs("\"elems\":[", output);
  if (ast->children)
    output_ast(output, ast->children);
  fputs("]}", output);
  if (ast->next) {
    fputc(',', output);
    output_ast(output, ast->next);
  }
}

int main(int argc, const char* argv[]) {
  if (argc < 3)
    exit(1);
  const char* output_file = argv[1];
  const char* input_file  = argv[2];

  FILE* input = fopen(input_file, "rb");
  if (!input)
    exit(-3);
  fseek(input, 0, SEEK_END);
  size_t size = ftell(input);
  fseek(input, 0, SEEK_SET);

  char* buffer = smalloc(size + 1);
  fread(buffer, 1, size, input);
  fclose(input);
  buffer[size] = '\0';

  TokenList tokens = {};
  int status       = tokenize(buffer, &tokens);

  // output_tokens(output_file, tokens, buffer);

  FILE* out_ast = fopen(output_file, "w");
  ASTNode* program;
  status |= parse(&tokens, &program);
  output_ast(out_ast, program);
  fclose(out_ast);

  return status;
}
