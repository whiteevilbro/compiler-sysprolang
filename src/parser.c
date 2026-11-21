// clang-format off
#define _XOPEN_SOURCE 600
#include <features.h>
// clang-format on

#include "parser.h"

#include "./memory/managment.h"
#include "hashmap/hashing.h"
#include "hashmap/hashmap.h"
#include "lexer.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>


typedef uint8_t bindingPower;

typedef struct {
  bindingPower lbp;
  bindingPower rbp;
} BPPair;

typedef enum : unsigned char {
  ADD      = 0,
  SUBTRACT = 1,
  MULTIPLY = 2,
  DIVIDE   = 3,
  ASSIGN   = 4,
  EQ       = 5,
  NE       = 6,
  LT       = 7,
  GT       = 8,
  LE       = 9,
  GE       = 10,
  AND      = 11,
  OR       = 12,
} OperatorKind;

static union {
  enum BinaryOperatorKind binary;
} operatorToASTKind[] = {
    [ADD]      = {.binary = BOK_ADD},
    [SUBTRACT] = {.binary = BOK_SUBTRACT},
    [MULTIPLY] = {.binary = BOK_MULTIPLY},
    [DIVIDE]   = {.binary = BOK_DIVIDE},
    [ASSIGN]   = {.binary = BOK_ASSIGN},
    [EQ]       = {.binary = BOK_EQ},
    [NE]       = {.binary = BOK_NE},
    [LT]       = {.binary = BOK_LT},
    [GT]       = {.binary = BOK_GT},
    [LE]       = {.binary = BOK_LE},
    [GE]       = {.binary = BOK_GE},
    [AND]      = {.binary = BOK_AND},
    [OR]       = {.binary = BOK_OR},
};

#define attachNext(prev, next) (prev).next = (next)
#define attachChild(parent, child) (parent).child = (child)

#define newNode() ((ASTNode*) smalloc(sizeof(ASTNode)))

// parsing
static int parse_return(Parser*, ASTNode**);
static int parse_decl(Parser*, ASTNode**);
static int parse_expr(Parser*, bindingPower, ASTNode**);
static int parse_statement(Parser*, ASTNode**, bool);
static int parse_if(Parser*, ASTNode**, bool);
static int parse_while(Parser*, ASTNode**);
static int parse_block(Parser*, ASTNode**, bool);

// creating various nodes
static inline ASTNode* new_program(size_t, const char*);
static ASTNode* make_error_node(const Token*);
static inline ASTNode* make_identifier_node(const Token*);
static inline ASTNode* make_literal_node(const Token*);
static inline ASTNode* make_unary_operator_node(const Token*);
static inline ASTNode* make_binary_operator_node(const Token*, OperatorKind);
static inline ASTNode* make_break_node(const Token*);
static inline ASTNode* make_continue_node(const Token*);
static inline ASTNode* make_decl_node(const Token*);

// helpers
static inline BPPair infix_binding_power(OperatorKind op);

void init_parser(Parser* parser, Lexer* lexer) {
  *parser = (Parser) {
      .lexer = lexer,
      .root  = NULL,
  };
  hashmap_cleanup(&parser->identifiers);
  hashmap_init(&parser->identifiers, string_hash, string_cmp);
}

int parse(Parser* parser, const char* source_filename) {
  int status   = 0;
  Lexer* lexer = parser->lexer;

  Token token;

  status           = peek_token(lexer, &token);
  ASTNode* program = new_program(token.offset, realpath(source_filename, NULL));
  parser->root     = program;

  // two-star programming
  ASTNode** last_statement = &(program->children);
  do {
    status |= parse_statement(parser, last_statement, false);
    if (*last_statement)
      last_statement = &((*last_statement)->next);
    peek_token(lexer, &token);
  } while (token.kind != TK_EOF);

  if (!program->children) {
    //todo log no statements error
    return status | 1;
  }
  last_statement = &(program->children);
  while ((*last_statement)->next) {
    last_statement = &((*last_statement)->next);
  }
  if ((*last_statement)->kind != NK_RETURN)
    status |= 1;

  return status;
}

static int parse_return(Parser* parser, ASTNode** node) {
  Token ret_token;
  get_token(parser->lexer, &ret_token);
  assert(ret_token.kind == TK_RETURN);

  ASTNode* ret_node = newNode();

  *ret_node  = (ASTNode) {.kind = NK_RETURN, .offset = ret_token.offset, .children = NULL, .next = NULL};
  int status = parse_expr(parser, 0, &ret_node->children);

  *node = ret_node;
  return status;
}

static int parse_decl(Parser* parser, ASTNode** node) {
  int status   = 0;
  Lexer* lexer = parser->lexer;

  Token token;
  get_token(lexer, &token);
  assert(token.kind == TK_VAL || token.kind == TK_VAR);

  ASTNode* decl_node = make_decl_node(&token);

  peek_token(lexer, &token);
  if (token.kind == TK_IDENTIFIER) {
    advance_token(lexer);
    decl_node->children = make_identifier_node(&token);
  } else {
    decl_node->children = make_error_node(&token);
    status |= 1;
  }

  peek_token(lexer, &token);
  if (token.kind == TK_OP_ASSIGN) {
    advance_token(lexer);
    status |= parse_expr(parser, 0, &decl_node->children->next);
  } else {
    decl_node->children->next = make_error_node(&token);
    status |= 1;
  }

  if (!status) {
    const char* key = (const char*) decl_node->children->data.asIndentifier.name;
    if (hashmap_get(&parser->identifiers, key)) {
      status |= 1; //todo log redefinition error
    } else {
      hashmap_insert(&parser->identifiers, key, decl_node);
    }
  }

  *node = decl_node;
  return status;
}

static int parse_if(Parser* parser, ASTNode** node, bool is_loop) {
  int status   = 0;
  Lexer* lexer = parser->lexer;

  Token token;
  get_token(lexer, &token);
  assert(token.kind == TK_IF);

  ASTNode* if_node = newNode();

  *if_node = (ASTNode) {
      .kind     = NK_IF,
      .offset   = token.offset,
      .next     = NULL,
      .children = NULL,
  };

  ASTNode** last_node = &if_node->children;
  peek_token(lexer, &token);
  if (token.kind == TK_LEFT_PARENTHESIS) {
    advance_token(lexer);

    status |= parse_expr(parser, 0, last_node);

    peek_token(lexer, &token);
    if (token.kind == TK_RIGHT_PARENTHESIS)
      advance_token(lexer);
    else
      status |= 1;
  } else {
    //? I think its fair to say that no parenthesis => no condition and everything that follows is a body, but whatever
    advance_token(lexer); //! for test with no parenthesis.
    *last_node = make_error_node(&token);
    status |= 1;
  }

  last_node = &((*last_node)->next);
  status |= parse_statement(parser, last_node, is_loop);
  if (!*last_node) //todo
    *last_node = make_error_node(&token);

  peek_token(lexer, &token);
  last_node = &((*last_node)->next);
  if (token.kind == TK_ELSE) {
    advance_token(lexer);
    status |= parse_statement(parser, last_node, is_loop);
    if (!*last_node)
      *last_node = make_error_node(&token);
  }

  *node = if_node;
  return status;
}

static int parse_while(Parser* parser, ASTNode** node) {
  int status   = 0;
  Lexer* lexer = parser->lexer;

  Token token;
  get_token(lexer, &token);
  assert(token.kind == TK_WHILE);

  ASTNode* while_node = newNode();

  *while_node = (ASTNode) {
      .kind     = NK_WHILE,
      .offset   = token.offset,
      .next     = NULL,
      .children = NULL,
  };

  ASTNode** cond_node = &while_node->children;
  get_token(lexer, &token);
  if (token.kind == TK_LEFT_PARENTHESIS) {
    status |= parse_expr(parser, 0, cond_node);

    get_token(lexer, &token);
    if (token.kind != TK_RIGHT_PARENTHESIS)
      status |= 1;
  } else {
    *cond_node = make_error_node(&token);
    status |= 1;
  }

  ASTNode** body = &(*cond_node)->next;
  status |= parse_statement(parser, body, true);

  *node = while_node;
  return status;
}

static int parse_block(Parser* parser, ASTNode** node, bool is_loop) {
  int status   = 0;
  Lexer* lexer = parser->lexer;

  Token token;
  get_token(lexer, &token);
  assert(token.kind == TK_BLOCK_START);

  ASTNode* block_node = newNode();

  *block_node = (ASTNode) {
      .kind     = NK_BLOCK,
      .offset   = token.offset,
      .next     = NULL,
      .children = NULL,
  };

  ASTNode** last_statement = &(block_node->children);
  while ((peek_token(lexer, &token), token.kind) != TK_BLOCK_END) {
    status |= parse_statement(parser, last_statement, is_loop);
    if (*last_statement)
      last_statement = &((*last_statement)->next);
  }
  advance_token(lexer);

  *node = block_node;
  return status;
}

static int parse_expr(Parser* parser, bindingPower min_bp, ASTNode** node) {
  __label__ end;

  int status   = 0;
  Lexer* lexer = parser->lexer;

  Token token;
  peek_token(lexer, &token);

  ASTNode* lhs;
  switch (token.kind) {
    case TK_IDENTIFIER:
      advance_token(lexer);
      lhs = make_identifier_node(&token);
      if (!hashmap_get((&parser->identifiers), (const char*) lhs->data.asIndentifier.name))
        status |= 1;
      break;

    case TK_INT_LITERAL:
    case TK_TRUE:
    case TK_FALSE:
      advance_token(lexer);
      lhs = make_literal_node(&token);
      break;

    case TK_LEFT_PARENTHESIS:
      advance_token(lexer);
      status |= parse_expr(parser, 0, &lhs);
      peek_token(lexer, &token);
      if (token.kind != TK_RIGHT_PARENTHESIS) {
        status |= 1;
        lhs = make_error_node(&token);
      } else {
        advance_token(lexer);
      }
      break;

    case TK_OP_MINUS:
    case TK_OP_NOT:
      bindingPower r_bp = 100; //TODO
      ASTNode* rhs;
      advance_token(lexer);
      lhs = make_unary_operator_node(&token);
      status |= parse_expr(parser, r_bp, &rhs);
      lhs->children = rhs;
      break;

    default:
      status |= 1;
      *node = make_error_node(&token);
      return status;
  }

  while (true) {
    peek_token(lexer, &token);
    OperatorKind op;
    switch (token.kind) {
      case TK_RIGHT_PARENTHESIS:
      case TK_EOS:
      case TK_EOF:
        goto end; // breaks the cycle

      case TK_OP_PLUS:
        op = ADD;
        break;
      case TK_OP_MINUS:
        op = SUBTRACT;
        break;
      case TK_OP_ASTERISK:
        op = MULTIPLY;
        break;
      case TK_OP_SLASH:
        op = DIVIDE;
        break;
      case TK_OP_ASSIGN:
        op = ASSIGN;
        break;
      case TK_OP_EQ:
        op = EQ;
        break;
      case TK_OP_NE:
        op = NE;
        break;
      case TK_OP_LT:
        op = LT;
        break;
      case TK_OP_GT:
        op = GT;
        break;
      case TK_OP_LE:
        op = LE;
        break;
      case TK_OP_GE:
        op = GE;
        break;
      case TK_OP_AND:
        op = AND;
        break;
      case TK_OP_OR:
        op = OR;
        break;

      default:
        status |= 1;
        *node = make_error_node(&token);
        return status;
    }

    // postfix block goes here

    ASTNode* res = make_binary_operator_node(&token, op);
    BPPair bp    = infix_binding_power(op);
    if (bp.lbp < min_bp)
      break;
    advance_token(lexer);

    ASTNode* rhs;
    status |= parse_expr(parser, bp.rbp, &rhs);
    res->children = lhs;
    lhs->next     = rhs;

    if (res->data.asBinaryOperator.kind == BOK_ASSIGN) {
      if (lhs->kind == NK_IDENTIFIER) {
        ASTNode* decl = hashmap_get(&parser->identifiers, (const char*) lhs->data.asIndentifier.name);
        if (!decl || decl->kind != NK_DECLARE || decl->data.asDeclaration.type == DT_VAL)
          status |= 1;
      }
    }

    lhs = res;
    // continue;
  }

end:

  *node = lhs;
  return status;
}

//todo: rethink behaviour regarding skipping in case of an error
static int parse_statement(Parser* parser, ASTNode** node, bool is_loop) {
  int status   = 0;
  Lexer* lexer = parser->lexer;

  Token token;
  peek_token(lexer, &token);
  switch (token.kind) {
    case TK_ERROR:
    case TK_EOF:
    case TK_EOS:
      advance_token(lexer); //todo: NOP node
      return status;

    case TK_VAL:
    case TK_VAR:
      status |= parse_decl(parser, node);
      break;

    case TK_RETURN:
      status |= parse_return(parser, node);
      break;

    case TK_IF:
      status |= parse_if(parser, node, is_loop);
      return status; // no ; needed
    case TK_WHILE:
      status |= parse_while(parser, node);
      return status; // no ; needed
    case TK_BREAK:
      status |= is_loop ? 0 : 1;
      *node = make_break_node(&token);
      advance_token(lexer);
      break;
    case TK_CONTINUE:
      status |= !is_loop;
      *node = make_continue_node(&token);
      advance_token(lexer);
      break;
    case TK_BLOCK_START:
      status = parse_block(parser, node, is_loop);
      return status;

    case TK_RIGHT_PARENTHESIS:
    case TK_LEFT_PARENTHESIS:
    case TK_INT_LITERAL:
    case TK_OP_MINUS:
    case TK_IDENTIFIER:
    case TK_OP_NOT:
    case TK_TRUE:
    case TK_FALSE:
      status |= parse_expr(parser, 0, node);
      break;

    case TK_NIL:
    case TK_OP_ASTERISK:
    case TK_OP_SLASH:
    case TK_OP_PLUS:
    case TK_OP_ASSIGN:
    case TK_ELSE:
    case TK_OP_EQ:
    case TK_OP_NE:
    case TK_OP_LT:
    case TK_OP_GT:
    case TK_OP_LE:
    case TK_OP_GE:
    case TK_OP_AND:
    case TK_OP_OR:
    case TK_BLOCK_END:
      status |= 1;
      advance_token(lexer);
      *node = make_error_node(&token);
      return status;
  }
  // ensure ;
  peek_token(lexer, &token);
  if (token.kind != TK_EOS) {
    status |= 1;

    //? maybe add skip to the end of the line?
    // skip to end of statement, block or file whichever is nearer
    while (token.kind != TK_EOS && token.kind != TK_BLOCK_END && token.kind != TK_EOF) {
      advance_token(lexer);
      peek_token(lexer, &token);
    }
    if (token.kind == TK_EOS)
      advance_token(lexer);
  } else {
    advance_token(lexer);
  }

  return status;
}

// Creating various nodes

static inline ASTNode* new_program(size_t offset, const char* filename) {
  ASTNode* node = (ASTNode*) smalloc(sizeof(ASTNode));

  *node = (ASTNode) {.kind = NK_PROGRAM, .offset = offset, .children = NULL, .next = NULL, .data.asProgram = {.filename = filename}};

  return node;
}

static ASTNode* make_error_node(const Token* token) {
  ASTNode* err_node = newNode();

  *err_node = (ASTNode) {
      .kind     = NK_ERROR,
      .offset   = token->offset,
      .next     = NULL,
      .children = NULL,
  };

  return err_node;
}

static inline ASTNode* make_identifier_node(const Token* token) {
  ASTNode* node = newNode();

  *node = (ASTNode) {
      .kind               = NK_IDENTIFIER,
      .offset             = token->offset,
      .next               = NULL,
      .children           = NULL,
      .data.asIndentifier = {.name = token->data.str},
  };

  return node;
}

static inline ASTNode* make_literal_node(const Token* token) {
  ASTNode* node = newNode();

  switch (token->kind) {
    case TK_INT_LITERAL:
      *node = (ASTNode) {
          .kind              = NK_INT_LITERAL,
          .offset            = token->offset,
          .next              = NULL,
          .children          = NULL,
          .data.asIntLiteral = {.value = token->data.value},
      };
      break;
    case TK_TRUE:
    case TK_FALSE:
      *node = (ASTNode) {
          .kind               = NK_BOOL_LITERAL,
          .offset             = token->offset,
          .next               = NULL,
          .children           = NULL,
          .data.asBoolLiteral = {.value = token->kind == TK_TRUE},
      };
      break;
    default:
      assert(false);
      __builtin_unreachable();
      break;
  }

  return node;
}

static inline ASTNode* make_unary_operator_node(const Token* token) {
  ASTNode* node = newNode();

  *node = (ASTNode) {
      .kind                 = NK_UNARY_OP,
      .offset               = token->offset,
      .next                 = NULL,
      .children             = NULL,
      .data.asUnaryOperator = {.kind = token->kind == TK_OP_MINUS ? UOK_NEG : UOK_NOT}};

  return node;
}

static inline ASTNode* make_binary_operator_node(const Token* token, OperatorKind op) {
  ASTNode* node = newNode();

  *node = (ASTNode) {
      .kind                  = NK_BINARY_OP,
      .offset                = token->offset,
      .next                  = NULL,
      .children              = NULL,
      .data.asBinaryOperator = {.kind = operatorToASTKind[op].binary},
  };

  return node;
}

static inline ASTNode* make_break_node(const Token* token) {
  ASTNode* node = newNode();

  *node = (ASTNode) {
      .kind     = NK_BREAK,
      .offset   = token->offset,
      .next     = NULL,
      .children = NULL,
  };

  return node;
}

static inline ASTNode* make_continue_node(const Token* token) {
  ASTNode* node = newNode();

  *node = (ASTNode) {
      .kind     = NK_CONTINUE,
      .offset   = token->offset,
      .next     = NULL,
      .children = NULL,
  };

  return node;
}

static inline ASTNode* make_decl_node(const Token* token) {
  ASTNode* node = newNode();

  *node = (ASTNode) {
      .kind               = NK_DECLARE,
      .offset             = token->offset,
      .next               = NULL,
      .children           = NULL,
      .data.asDeclaration = {.type = token->kind == TK_VAL ? DT_VAL : DT_VAR},
  };

  return node;
}

// helpers

static inline BPPair infix_binding_power(OperatorKind op) {
  switch (op) {
    case ASSIGN:
      return (BPPair) {2, 1};
    case OR:
      return (BPPair) {3, 4};
    case AND:
      return (BPPair) {5, 6};
    case EQ:
    case NE:
      return (BPPair) {7, 8};
    case LT:
    case GT:
    case LE:
    case GE:
      return (BPPair) {9, 10};
    case ADD:
    case SUBTRACT:
      return (BPPair) {11, 12};
    case MULTIPLY:
    case DIVIDE:
      return (BPPair) {13, 14};
  }
}
