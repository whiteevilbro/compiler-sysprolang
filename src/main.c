#include "input.h"
#include "lexer.h"
#include "output.h"
#include "parser.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
  const char* input_file  = NULL;
  const char* lexer_file  = NULL;
  const char* parser_file = NULL;

  int opt;
  while ((opt = getopt(argc, argv, "l:p:f:")) != -1) {
    switch (opt) {
      case 'l':
        lexer_file = optarg;
        break;
      case 'p':
        parser_file = optarg;
        break;
      case 'f':
        input_file = optarg;
        break;
      default:
usage:
        printf("Usage: splc [-l lexer_outfile] [-p parser_outfile] [-f infile]");
        exit(-1);
    }
  }

  SizeVec newlines = {};

  if (!input_file)
    goto usage;
  const char* buffer = read_file(input_file, &newlines);
  if (!buffer) {
    printf("Couln't read file: %s\n", input_file);
    exit(2);
  }

  TokenList tokens = {};
  int status       = tokenize(buffer, &tokens);

  if (lexer_file)
    output_tokens(lexer_file, tokens, &newlines);

  ASTNode* program;
  status |= parse(&tokens, &program);

  if (parser_file)
    output_ast(parser_file, program, &newlines);

  return status;
}
