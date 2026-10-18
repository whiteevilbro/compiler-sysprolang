#include "codegen.h"

#include "hashmap/hashing.h"
#include "hashmap/hashmap.h"
#include "memory/managment.h"
#include "parser.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIZE_T_MAX_DIGITS 19

typedef struct {
  const char* ir_name;
  size_t uses;
} IdentifierMeta;

HashmapDef(char, IdentifierMeta) IdentMetaTable;

static inline int generate_statement(FILE*, ASTNode*, IdentMetaTable*);
static int generate_return(FILE*, ASTNode*, IdentMetaTable*);
static int generate_declaration(FILE*, ASTNode*, IdentMetaTable*);
static int generate_expression(FILE*, ASTNode*, IdentMetaTable*, const char**);

static inline int generate_literal(ASTNode*, const char**);
static inline int generate_identifier(FILE*, ASTNode*, IdentMetaTable*, const char**);
static inline int generate_unary(FILE*, ASTNode*, IdentMetaTable*, const char**);
static inline int generate_binary(FILE*, ASTNode*, IdentMetaTable*, const char**);
static inline int generate_assignment(FILE*, ASTNode*, IdentMetaTable*, const char**);

int generate_llvm_ir(FILE* file, ASTNode* ast) {
  if (!ast || ast->kind != NK_PROGRAM)
    return -1;

  IdentMetaTable ident_meta;
  // hashmap_cleanup(&ident_meta);
  hashmap_init(&ident_meta, string_hash, string_cmp);

  fprintf(file,
          "; ModuleID = 'spl'\n"
          "source_filename = \"%s\"\n"
          "target datalayout = \"e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128\"\n"
          "target triple = \"x86_64-unknown-linux-gnu\"\n"
          "\n"
          "define i64 @main() {\n"
          "entry:\n",
          ast->data.asProgram.filename);

  int status     = 0;
  ASTNode** next = &ast->children;
  while (*next) {
    status |= generate_statement(file, *next, &ident_meta);
    next = &((*next)->next);
  }

  fputs("}\n", file);

  return status;
}

static inline int generate_statement(FILE* file, ASTNode* node, IdentMetaTable* meta) {
  switch (node->kind) {
    case NK_RETURN:
      return generate_return(file, node, meta);
    case NK_DECLARE:
      return generate_declaration(file, node, meta);
    case NK_INT_LITERAL:
    case NK_IDENTIFIER:
    case NK_BINARY_OP:
    case NK_UNARY:
      return generate_expression(file, node, meta, NULL);

    case NK_PROGRAM:
    case NK_NIL:
    case NK_ERROR:
    default:
      return -2;
  }
  return 0;
}

static int generate_return(FILE* file, ASTNode* node, IdentMetaTable* meta) {
  const char* result_name = NULL;

  int status = generate_expression(file, node->children, meta, &result_name);

  fprintf(file, "ret i64 %s\n", result_name);

  free((void*) result_name);

  return status;
}

static int generate_declaration(FILE* file, ASTNode* node, IdentMetaTable* meta) {
  ASTNode* ident = node->children;
  ASTNode* init  = node->children->next;

  const char* idname = ident->data.asIndentifier.name;
  size_t name_length = strlen(idname) + 2;
  char* ir_name      = (char*) smalloc((name_length) * sizeof(char));
  snprintf(ir_name, name_length, "%%%s", idname); // just a precaution

  IdentifierMeta* metadata = (IdentifierMeta*) smalloc(sizeof(IdentifierMeta));
  *metadata                = (IdentifierMeta) {.uses = 0, .ir_name = ir_name};

  hashmap_insert(meta, idname, metadata);

  fprintf(file, "%s = alloca i64, align 8\n", ir_name);

  const char* init_name = NULL;
  int status            = generate_expression(file, init, meta, &init_name);

  fprintf(file, "store i64 %s, ptr %s, align 8\n", init_name, ir_name); // kinda collides with generating assignment, but will differ for vals, someday

  free((void*) init_name);

  return status;
}

static int generate_expression(FILE* file, ASTNode* node, IdentMetaTable* meta, const char** result_name) {
  switch (node->kind) {
    case NK_INT_LITERAL:
      return generate_literal(node, result_name);
    case NK_IDENTIFIER:
      return generate_identifier(file, node, meta, result_name);
    case NK_BINARY_OP:
      return generate_binary(file, node, meta, result_name);
    case NK_UNARY:
      return generate_unary(file, node, meta, result_name);

    case NK_NIL:
    case NK_ERROR:
    case NK_PROGRAM:
    case NK_RETURN:
    case NK_DECLARE:
    default:
      if (result_name)
        *result_name = NULL;
      return -1;
  }
  return -2; // should be unreachable
}

static inline int generate_literal(ASTNode* node, const char** result_name) {
  assert(node->kind == NK_INT_LITERAL);

  if (result_name) {
    char buf[SIZE_T_MAX_DIGITS + 1];
    snprintf(buf, sizeof(buf) / sizeof(buf[0]), "%ld", node->data.asIntLiteral.value);
    size_t len   = strlen(buf) + 1;
    char* val    = (char*) smalloc(len * sizeof(char));
    *result_name = strcpy(val, buf);
  }

  return 0;
}

static inline int generate_identifier(FILE* file, ASTNode* node, IdentMetaTable* meta, const char** result_name) {
  assert(node->kind == NK_IDENTIFIER);

  // there could be val fastpath

  IdentifierMeta* metadata = hashmap_get(meta, (const char*) node->data.asIndentifier.name);

  fprintf(file, "%s.%zu = load i64, ptr %s, align 8\n", metadata->ir_name, ++metadata->uses, metadata->ir_name);

  if (result_name) {
    char buf[SIZE_T_MAX_DIGITS + 1];
    snprintf(buf, sizeof(buf) / sizeof(buf[0]), "%ld", metadata->uses);

    size_t len = strlen(metadata->ir_name) + 1 + strlen(buf) + 1;
    char* name = (char*) smalloc(len * sizeof(char));
    snprintf(name, len, "%s.%zu", metadata->ir_name, metadata->uses);

    *result_name = name;
  }

  return 0;
}

static inline int generate_unary(FILE* file, ASTNode* node, IdentMetaTable* meta, const char** result_name) {
  assert(node->kind == NK_UNARY);

  static size_t counter = 0;

  const char* result = NULL;
  int status         = generate_expression(file, node->children, meta, &result);

  fprintf(file, "%%.negtmp.%zu = sub i64 0, %s\n", counter, result);

  free((void*) result);

  if (result_name) {
    char buf[SIZE_T_MAX_DIGITS + 1];
    snprintf(buf, sizeof(buf) / sizeof(buf[0]), "%ld", counter);

    size_t len = sizeof("%.negtmp.") + strlen(buf);
    char* name = (char*) smalloc(len * sizeof(char));
    snprintf(name, len, "%%.negtmp.%zu", counter);

    *result_name = name;
  }
  counter++;

  return status;
}

static inline int generate_binary(FILE* file, ASTNode* node, IdentMetaTable* meta, const char** result_name) {
  assert(node->kind == NK_BINARY_OP);

  int status            = 0;
  static size_t counter = 0;

  if (node->data.asBinaryOperator.kind == BOK_ASSIGN)
    return generate_assignment(file, node, meta, result_name);

  const char* cmd = NULL;
  switch (node->data.asBinaryOperator.kind) {
    case BOK_ADD:
      cmd = "add";
      break;
    case BOK_SUBTRACT:
      cmd = "sub";
      break;
    case BOK_MULTIPLY:
      cmd = "mul";
      break;
    case BOK_DIVIDE:
      cmd = "sdiv";
      break;

    default:
      if (result_name)
        *result_name = NULL;
      return -1;
  }

  ASTNode* lhs = node->children;
  ASTNode* rhs = node->children->next;

  const char* lhs_name = NULL;
  const char* rhs_name = NULL;

  status |= generate_expression(file, lhs, meta, &lhs_name);
  status |= generate_expression(file, rhs, meta, &rhs_name);

  fprintf(file, "%%.tmp.%zu = %s i64 %s, %s\n", counter, cmd, lhs_name, rhs_name);

  free((void*) lhs_name);
  free((void*) rhs_name);

  if (result_name) {
    char buf[SIZE_T_MAX_DIGITS + 1];
    snprintf(buf, sizeof(buf) / sizeof(buf[0]), "%ld", counter);

    size_t len = sizeof("%.tmp.") + strlen(buf);
    char* name = (char*) smalloc(len * sizeof(char));
    snprintf(name, len, "%%.tmp.%zu", counter);

    *result_name = name;
  }

  counter++;

  return status;
}

static inline int generate_assignment(FILE* file, ASTNode* node, IdentMetaTable* meta, const char** result_name) {
  assert(node->kind == NK_BINARY_OP && node->data.asBinaryOperator.kind == BOK_ASSIGN);

  ASTNode* lhs = node->children;
  ASTNode* rhs = node->children->next;

  assert(lhs->kind == NK_IDENTIFIER);

  IdentifierMeta* metadata = hashmap_get(meta, (const char*) lhs->data.asIndentifier.name);

  const char* rhs_name = NULL;
  int status           = generate_expression(file, rhs, meta, &rhs_name);

  fprintf(file, "store i64 %s, ptr %s, align 8", rhs_name, metadata->ir_name);

  if (result_name)
    *result_name = rhs_name;
  else
    free((void*) rhs_name);

  return status;
}
