// clang-format off
#define _XOPEN_SOURCE 600
#include <features.h>
// clang-format on

#include "parser.h"

#include "./memory/managment.h"
#include "hashmap/hashing.h"
#include "hashmap/hashmap.h"
#include "lexer.h"
#include "vector/vector.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>


typedef uint8_t bindingPower;

HashmapDef(char, ASTNode) map;

static map idmap;

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
    [OR]       = {.binary = BOK_OR}};

#define attachNext(prev, next) (prev).next = (next)
#define attachChild(parent, child) (parent).child = (child)

#define newNode() ((ASTNode*) smalloc(sizeof(ASTNode)))

static int parse_return(TokenList*, size_t*, ASTNode**);
static int parse_decl(TokenList*, size_t*, ASTNode**);
static int parse_expr(TokenList*, size_t*, bindingPower, ASTNode**);
static int parse_statement(TokenList*, size_t*, ASTNode**, bool);
static int parse_if(TokenList*, size_t*, ASTNode**, bool);
static int parse_while(TokenList*, size_t*, ASTNode**);
static int parse_block(TokenList*, size_t*, ASTNode**, bool);

static inline ASTNode* make_identifier_node(Token* token);

static size_t skip_to(TokenList* token_list, size_t offset, enum TokenKind kind) {
  Token* token = vecGetPtr(token_list, offset);
  while (token->kind != kind && token->kind != TK_EOF) {
    offset++;
    token = vecGetPtr(token_list, offset);
  }
  return offset;
}

static inline ASTNode* new_program(size_t offset, const char* filename) {
  ASTNode* node = (ASTNode*) smalloc(sizeof(ASTNode));

  *node = (ASTNode) {.kind = NK_PROGRAM, .offset = offset, .children = NULL, .next = NULL, .data.asProgram = {.filename = filename}};

  return node;
}

static ASTNode* make_error_node(Token* token) {
  ASTNode* err_node = newNode();

  *err_node = (ASTNode) {
      .kind     = NK_ERROR,
      .offset   = token->offset,
      .next     = NULL,
      .children = NULL,
  };

  return err_node;
}

static int parse_return(TokenList* token_list, size_t* offset, ASTNode** node) {
  Token* ret_token = vecGetPtr(token_list, *offset);
  assert(ret_token->kind == TK_RETURN);

  ASTNode* ret_node = newNode();

  *ret_node = (ASTNode) {.kind = NK_RETURN, .offset = ret_token->offset, .children = NULL, .next = NULL};
  (*offset)++;
  int status = parse_expr(token_list, offset, 0, &ret_node->children);

  *node = ret_node;
  return status;
}

static int parse_decl(TokenList* token_list, size_t* offset, ASTNode** node) {
  int status = 0;

  Token* token = vecGetPtr(token_list, *offset);
  assert(token->kind == TK_VAL || token->kind == TK_VAR);

  ASTNode* decl_node = newNode();

  *decl_node = (ASTNode) {
      .kind               = NK_DECLARE,
      .offset             = token->offset,
      .next               = NULL,
      .children           = NULL,
      .data.asDeclaration = {.type = token->kind == TK_VAL ? DT_VAL : DT_VAR},
  };

  (*offset)++;

  token = vecGetPtr(token_list, *offset);
  if (token->kind == TK_IDENTIFIER)
    decl_node->children = make_identifier_node(token);
  else {
    decl_node->children = make_error_node(token);
    status |= 1;
  }

  (*offset)++;
  token = vecGetPtr(token_list, *offset);
  if (token->kind != TK_OP_ASSIGN) {
    decl_node->children->next = make_error_node(token);
    status |= 1;
  } else {
    (*offset)++;
    status |= parse_expr(token_list, offset, 0, &decl_node->children->next);
  }

  if (!status) {
    const char* key = (const char*) decl_node->children->data.asIndentifier.name;
    if (hashmap_get(&idmap, key)) {
      status |= 1;
    } else {
      // IdentifierMeta* data = (IdentifierMeta*) smalloc(sizeof(IdentifierMeta));
      // *data                = (IdentifierMeta) {.kind = decl_node->data.asDeclaration.type};
      hashmap_insert(&idmap, key, decl_node);
    }
  }

  *node = decl_node;
  return status;
}

static int parse_if(TokenList* token_list, size_t* offset, ASTNode** node, bool is_loop) {
  int status = 0;

  Token* token = vecGetPtr(token_list, *offset);
  assert(token->kind == TK_IF);

  ASTNode* if_node = newNode();

  *if_node = (ASTNode) {
      .kind     = NK_IF,
      .offset   = token->offset,
      .next     = NULL,
      .children = NULL,
  };

  (*offset)++;

  ASTNode** last_node = &if_node->children;
  token               = vecGetPtr(token_list, *offset);
  if (token->kind == TK_LEFT_PARENTHESIS) {
    (*offset)++;
    status |= parse_expr(token_list, offset, 0, last_node);

    token = vecGetPtr(token_list, *offset);
    if (token->kind != TK_RIGHT_PARENTHESIS)
      status |= 1;
    else
      (*offset)++;
  } else {
    *last_node = make_error_node(token);
    status |= 1;
    (*offset)++;
  }

  token     = vecGetPtr(token_list, *offset);
  last_node = &((*last_node)->next);
  status |= parse_statement(token_list, offset, last_node, is_loop);
  if (!*last_node) //todo
    *last_node = make_error_node(token);

  token     = vecGetPtr(token_list, *offset);
  last_node = &((*last_node)->next);
  if (token->kind == TK_ELSE) {
    (*offset)++;
    status |= parse_statement(token_list, offset, last_node, is_loop);
    if (!*last_node)
      *last_node = make_error_node(token);
  }

  *node = if_node;
  return status;
}

static int parse_while(TokenList* token_list, size_t* offset, ASTNode** node) {
  int status = 0;

  Token* token = vecGetPtr(token_list, *offset);
  assert(token->kind == TK_WHILE);

  ASTNode* while_node = newNode();

  *while_node = (ASTNode) {
      .kind     = NK_WHILE,
      .offset   = token->offset,
      .next     = NULL,
      .children = NULL,
  };

  (*offset)++;

  ASTNode** cond_node = &while_node->children;
  token               = vecGetPtr(token_list, *offset);
  if (token->kind == TK_LEFT_PARENTHESIS) {
    (*offset)++;
    status |= parse_expr(token_list, offset, 0, cond_node);

    token = vecGetPtr(token_list, *offset);
    if (token->kind != TK_RIGHT_PARENTHESIS)
      status |= 1;
    else
      (*offset)++;
  } else {
    *cond_node = make_error_node(token);
    status |= 1;
    (*offset)++;
  }

  ASTNode** body = &(*cond_node)->next;
  status |= parse_statement(token_list, offset, body, true);

  *node = while_node;
  return status;
}

static int parse_block(TokenList* token_list, size_t* offset, ASTNode** node, bool is_loop) {
  int status = 0;

  Token* token = vecGetPtr(token_list, *offset);
  assert(token->kind == TK_BLOCK_START);
  ASTNode* block_node = newNode();

  *block_node = (ASTNode) {
      .kind     = NK_BLOCK,
      .offset   = token->offset,
      .next     = NULL,
      .children = NULL,
  };

  (*offset)++;

  ASTNode** last_statement = &(block_node->children);
  while ((token = vecGetPtr(token_list, *offset))->kind != TK_BLOCK_END) {
    status |= parse_statement(token_list, offset, last_statement, is_loop);
    if (*last_statement)
      last_statement = &((*last_statement)->next);
  }

  (*offset)++;
  *node = block_node;
  return status;
}

static inline ASTNode* make_identifier_node(Token* token) {
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

static inline ASTNode* make_literal_node(Token* token) {
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

static inline ASTNode* make_unary_operator_node(Token* token) {
  ASTNode* node = newNode();

  *node = (ASTNode) {
      .kind                 = NK_UNARY_OP,
      .offset               = token->offset,
      .next                 = NULL,
      .children             = NULL,
      .data.asUnaryOperator = {.kind = token->kind == TK_OP_MINUS ? UOK_NEG : UOK_NOT}};

  return node;
}

static inline ASTNode* make_binary_operator_node(Token* token, OperatorKind op) {
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

static inline ASTNode* make_break_node(Token* token) {
  ASTNode* node = newNode();

  *node = (ASTNode) {
      .kind     = NK_BREAK,
      .offset   = token->offset,
      .next     = NULL,
      .children = NULL,
  };

  return node;
}

static inline ASTNode* make_continue_node(Token* token) {
  ASTNode* node = newNode();

  *node = (ASTNode) {
      .kind     = NK_CONTINUE,
      .offset   = token->offset,
      .next     = NULL,
      .children = NULL,
  };

  return node;
}

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

static int parse_expr(TokenList* token_list, size_t* offset, bindingPower min_bp, ASTNode** node) {
  __label__ end;

  int status = 0;

  Token* token = vecGetPtr(token_list, *offset);
  (*offset)++;

  ASTNode* lhs;
  switch (token->kind) {
    case TK_IDENTIFIER:
      lhs = make_identifier_node(token);
      if (!hashmap_get((&idmap), (const char*) lhs->data.asIndentifier.name))
        status |= 1;
      break;

    case TK_INT_LITERAL:
    case TK_TRUE:
    case TK_FALSE:
      lhs = make_literal_node(token);
      break;

    case TK_LEFT_PARENTHESIS:
      status |= parse_expr(token_list, offset, 0, &lhs);
      token = vecGetPtr(token_list, *offset);
      if (token->kind != TK_RIGHT_PARENTHESIS) {
        status |= 1;
        lhs = make_error_node(token);
      }
      (*offset)++;
      break;

    case TK_OP_MINUS:
    case TK_OP_NOT:
      bindingPower r_bp = 100; //TODO
      ASTNode* rhs;
      status |= parse_expr(token_list, offset, r_bp, &rhs);
      lhs           = make_unary_operator_node(token);
      lhs->children = rhs;
      break;

    default:
      status |= 1;
      *node = make_error_node(token);
      return status;
  }

  while (true) {
    token = vecGetPtr(token_list, *offset);
    OperatorKind op;
    switch (token->kind) {
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
        *node = make_error_node(token);
        return status;
    }

    // postfix block goes here

    BPPair bp = infix_binding_power(op);
    if (bp.lbp < min_bp)
      break;
    (*offset)++;
    ASTNode* rhs;
    status |= parse_expr(token_list, offset, bp.rbp, &rhs);
    ASTNode* res  = make_binary_operator_node(token, op);
    res->children = lhs;
    lhs->next     = rhs;

    if (res->data.asBinaryOperator.kind == BOK_ASSIGN) {
      if (lhs->kind == NK_IDENTIFIER) {
        ASTNode* decl = hashmap_get(&idmap, (const char*) lhs->data.asIndentifier.name);
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
static int parse_statement(TokenList* token_list, size_t* offset, ASTNode** node, bool is_loop) {
  int status = 0;

  Token* token = vecGetPtr(token_list, *offset);
  switch (token->kind) {
    case TK_ERROR:
    case TK_EOF:
    case TK_EOS:
      (*offset)++; //todo: NOP node
      return status;

    case TK_VAL:
    case TK_VAR:
      status |= parse_decl(token_list, offset, node);
      break;

    case TK_RETURN:
      status |= parse_return(token_list, offset, node);
      break;

    case TK_IF:
      status |= parse_if(token_list, offset, node, is_loop);
      return status; // no ; needed
    case TK_WHILE:
      status |= parse_while(token_list, offset, node);
      return status; // no ; needed
    case TK_BREAK:
      status |= is_loop ? 0 : 1;
      *node = make_break_node(token);
      (*offset)++;
      break;
    case TK_CONTINUE:
      status |= !is_loop;
      *node = make_continue_node(token);
      (*offset)++;
      break;
    case TK_BLOCK_START:
      status = parse_block(token_list, offset, node, is_loop);
      return status;

    case TK_RIGHT_PARENTHESIS:
    case TK_LEFT_PARENTHESIS:
    case TK_INT_LITERAL:
    case TK_OP_MINUS:
    case TK_IDENTIFIER:
    case TK_OP_NOT:
    case TK_TRUE:
    case TK_FALSE:
      status |= parse_expr(token_list, offset, 0, node);
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
      (*offset)++;
      *node = make_error_node(token);
      return status;
  }
  // ensure ;
  token = vecGetPtr(token_list, *offset);
  if (token->kind != TK_EOS) {
    status |= 1;
    *offset = skip_to(token_list, *offset, TK_EOS);
    token   = vecGetPtr(token_list, *offset);
    if (token->kind != TK_EOF)
      (*offset)++;
  } else {
    (*offset)++;
  }

  return status;
}

int parse(TokenList* token_list, ASTNode** nodep, const char* source_filename) {
  int status = 0;

  hashmap_cleanup(&idmap);
  hashmap_init(&idmap, string_hash, string_cmp);

  Token* token     = vecGetPtr(token_list, 0);
  ASTNode* program = new_program(token->offset, realpath(source_filename, NULL));
  *nodep           = program;

  size_t size    = vecSize(token_list);
  size_t current = 0;

  // two-star programming
  ASTNode** last_statement = &(program->children);
  do {
    status |= parse_statement(token_list, &current, last_statement, false);
    if (*last_statement)
      last_statement = &((*last_statement)->next);
  } while (token->kind != TK_EOF && current < size);

  last_statement = &(program->children);
  while ((*last_statement)->next) {
    last_statement = &((*last_statement)->next);
  }
  if ((*last_statement)->kind != NK_RETURN)
    status |= 1;

  return status;
}
