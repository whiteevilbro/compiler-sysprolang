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

typedef struct {
  enum DeclarationType kind : 2;
} IdentifierMeta;

HashmapDef(char, IdentifierMeta) map;

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
} OperatorKind;

static union {
  enum BinaryOperatorKind binary;
} operatorToASTKind[] = {
    [ADD]      = {.binary = BOK_ADD},
    [SUBTRACT] = {.binary = BOK_SUBTRACT},
    [MULTIPLY] = {.binary = BOK_MULTIPLY},
    [DIVIDE]   = {.binary = BOK_DIVIDE},
    [ASSIGN]   = {.binary = BOK_ASSIGN},
};

#define attachNext(prev, next) (prev).next = (next)
#define attachChild(parent, child) (parent).child = (child)

#define newNode() ((ASTNode*) smalloc(sizeof(ASTNode)))

static int parse_return(TokenList*, size_t*, ASTNode**);
static int parse_decl(TokenList*, size_t*, ASTNode**);
static int parse_expr(TokenList*, size_t*, bindingPower, ASTNode**);

static inline ASTNode* make_identifier_node(Token* token);

static size_t skip_to(TokenList* token_list, size_t offset, enum TokenKind kind) {
  Token* token = vecGetPtr(token_list, offset);
  while (token->kind != kind && token->kind != TK_EOF) {
    offset++;
    token = vecGetPtr(token_list, offset);
  }
  return offset;
}

static inline ASTNode* new_program(size_t offset) {
  ASTNode* node = (ASTNode*) smalloc(sizeof(ASTNode));

  *node = (ASTNode) {.kind = NK_PROGRAM, .offset = offset, .children = NULL, .next = NULL};

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
      IdentifierMeta* data = (IdentifierMeta*) smalloc(sizeof(IdentifierMeta));
      *data                = (IdentifierMeta) {.kind = decl_node->data.asDeclaration.type};
      hashmap_insert(&idmap, key, data);
    }
  }

  *node = decl_node;
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

  *node = (ASTNode) {
      .kind              = NK_INT_LITERAL,
      .offset            = token->offset,
      .next              = NULL,
      .children          = NULL,
      .data.asIntLiteral = {.value = token->data.value},
  };

  return node;
}

static inline ASTNode* make_unary_operator_node(Token* token) {
  ASTNode* node = newNode();

  *node = (ASTNode) {
      .kind     = NK_UNARY,
      .offset   = token->offset,
      .next     = NULL,
      .children = NULL,
  };

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

static inline BPPair infix_binding_power(OperatorKind op) {
  switch (op) {
    case ADD:
    case SUBTRACT:
      return (BPPair) {3, 4};
    case MULTIPLY:
    case DIVIDE:
      return (BPPair) {5, 6};
    case ASSIGN:
      return (BPPair) {2, 1};
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
        IdentifierMeta* data = hashmap_get(&idmap, (const char*) lhs->data.asIndentifier.name);
        if (!data || data->kind == DT_VAL)
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

static inline int string_cmp(const void* s1, const void* s2) { return strcmp(s1, s2); }

int parse(TokenList* token_list, ASTNode** nodep) {
  int status = 0;

  hashmap_cleanup(&idmap);
  hashmap_init(&idmap, string_hash, string_cmp);

  Token* token     = vecGetPtr(token_list, 0);
  ASTNode* program = new_program(token->offset);
  *nodep           = program;

  size_t size    = vecSize(token_list);
  size_t current = 0;

  // two-star programming
  ASTNode** last_statement = &(program->children);
  do {
    token = vecGetPtr(token_list, current);
    switch (token->kind) {
      case TK_ERROR:
      case TK_EOF:
      case TK_EOS:
        current++;
        continue;

      case TK_VAL:
      case TK_VAR:
        status |= parse_decl(token_list, &current, last_statement);
        last_statement = &((*last_statement)->next);
        break;

      case TK_RETURN:
        status |= parse_return(token_list, &current, last_statement);
        last_statement = &((*last_statement)->next);
        break;

      case TK_RIGHT_PARENTHESIS:
      case TK_LEFT_PARENTHESIS:
      case TK_INT_LITERAL:
      case TK_OP_MINUS:
      case TK_IDENTIFIER:
        status |= parse_expr(token_list, &current, 0, last_statement);
        last_statement = &((*last_statement)->next);
        break;

      case TK_NIL:
      case TK_OP_ASTERISK:
      case TK_OP_SLASH:
      case TK_OP_PLUS:
      case TK_OP_ASSIGN:
        status |= 1;
        current++;
        continue;
    }
    // ensure ;
    token = vecGetPtr(token_list, current);
    if (token->kind != TK_EOS) {
      status |= 1;
      current = skip_to(token_list, current, TK_EOS);
      token   = vecGetPtr(token_list, current);
      if (token->kind != TK_EOF)
        current++;
    }

  } while (token->kind != TK_EOF && current < size);

  last_statement = &(program->children);
  while ((*last_statement)->next) {
    last_statement = &((*last_statement)->next);
  }
  if ((*last_statement)->kind != NK_RETURN)
    status |= 1;

  return status;
}
