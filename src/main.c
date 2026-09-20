#include "lexer.h"
#include "memory/managment.h"
#include "vector/vector.h"

#include <stdio.h>
#include <stdlib.h>

static const char* token_string[] = {
    [NIL]   = "NIL",
    [T_EOF] = "EOF",
    [EOS]   = "SEMI",
    [ERROR] = "ERROR",

    [IDENTIFIER] = "IDENT",
    [VAL]        = "VAL",
    [VAR]        = "VAR",
    [RETURN]     = "RETURN",

    [RIGHT_PARENTHESIS] = "RPAREN",
    [LEFT_PARENTHESIS]  = "LPAREN",

    [OP_PLUS]     = "PLUS",
    [OP_MINUS]    = "MINUS",
    [OP_ASTERISK] = "MULT",
    [OP_SLASH]    = "DIV",
    [OP_ASSIGN]   = "ASSIGN",

    [INT_LITERAL] = "INT",
};

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
      if (buffer[bi++] == '\n') {
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

  return status;
}
