#include "codegen.h"

#include "hashmap/hashing.h"
#include "hashmap/hashmap.h"
#include "memory/managment.h"
#include "parser.h"

#include <alloca.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIZE_T_MAX_DIGITS 19

typedef struct {
  const char* ir_name;
  size_t uses;
} IdentifierMeta;

HashmapDef(char, IdentifierMeta) IdentMetaTable;

static inline int generate_statement(FILE*, ASTNode*, IdentMetaTable*, const char*, const char*);
static int generate_return(FILE*, ASTNode*, IdentMetaTable*);
static int generate_declaration(FILE*, ASTNode*, IdentMetaTable*);
static int generate_expression(FILE*, ASTNode*, IdentMetaTable*, const char**);

static inline int generate_int_literal(ASTNode*, const char**);
static inline int generate_identifier(FILE*, ASTNode*, IdentMetaTable*, const char**);
static inline int generate_unary(FILE*, ASTNode*, IdentMetaTable*, const char**);
static inline int generate_binary(FILE*, ASTNode*, IdentMetaTable*, const char**);
static inline int generate_assignment(FILE*, ASTNode*, IdentMetaTable*, const char**);
static inline int generate_if(FILE*, ASTNode*, IdentMetaTable*, const char*, const char*); // for now, if is not an expression
static inline int generate_while(FILE*, ASTNode*, IdentMetaTable*);                        // while is not an expression for now too
static inline int generate_branch(FILE*, const char*);
static inline int generate_bool_literal(ASTNode*, const char**);
static inline int generate_block(FILE*, ASTNode*, IdentMetaTable*, const char*, const char*);

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
    status |= generate_statement(file, *next, &ident_meta, NULL, NULL);
    next = &((*next)->next);
  }

  fputs("}\n", file);

  return status;
}

static inline int generate_statement(FILE* file, ASTNode* node, IdentMetaTable* meta, const char* enclosing_loop_start, const char* enclosing_loop_end) {
  switch (node->kind) {
    case NK_RETURN:
      return generate_return(file, node, meta);
    case NK_DECLARE:
      return generate_declaration(file, node, meta);
    case NK_INT_LITERAL:
    case NK_IDENTIFIER:
    case NK_BINARY_OP:
    case NK_UNARY_OP:
    case NK_BOOL_LITERAL:
      return generate_expression(file, node, meta, NULL);

    case NK_PROGRAM:
    case NK_NIL:
    case NK_ERROR:
    default:
      return -2;
    case NK_IF:
      return generate_if(file, node, meta, enclosing_loop_start, enclosing_loop_end);
    case NK_WHILE:
      return generate_while(file, node, meta);
    case NK_BREAK:
      return generate_branch(file, enclosing_loop_end);
    case NK_CONTINUE:
      return generate_branch(file, enclosing_loop_start);
    case NK_BLOCK:
      return generate_block(file, node, meta, enclosing_loop_start, enclosing_loop_end);
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
      return generate_int_literal(node, result_name);
    case NK_IDENTIFIER:
      return generate_identifier(file, node, meta, result_name);
    case NK_BINARY_OP:
      return generate_binary(file, node, meta, result_name);
    case NK_UNARY_OP:
      return generate_unary(file, node, meta, result_name);
    case NK_BOOL_LITERAL:
      return generate_bool_literal(node, result_name);

    default:
      if (result_name)
        *result_name = NULL;
      return -1;
      break;
  }
  return -2; // should be unreachable
}

static inline int generate_int_literal(ASTNode* node, const char** result_name) {
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

static inline int generate_bool_literal(ASTNode* node, const char** result_name) {
  assert(node->kind = NK_BOOL_LITERAL);

  if (result_name) {
    char* val    = (char*) smalloc(2 * sizeof(char));
    val[0]       = node->data.asBoolLiteral.value ? '1' : '0';
    val[1]       = '\0';
    *result_name = val;
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
  assert(node->kind == NK_UNARY_OP);

  static size_t counter = 0;

  const char* result = NULL;
  int status         = generate_expression(file, node->children, meta, &result);

  if (node->data.asUnaryOperator.kind == UOK_NEG)
    fprintf(file, "%%.utmp.%zu = sub i64 0, %s\n", counter, result);
  else
    fprintf(file, "%%.utmp.%zu = xor i64 1, %s\n", counter, result);

  free((void*) result);

  if (result_name) {
    char buf[SIZE_T_MAX_DIGITS + 1];
    snprintf(buf, sizeof(buf) / sizeof(buf[0]), "%ld", counter);

    size_t len = sizeof("%.negtmp.") + strlen(buf);
    char* name = (char*) smalloc(len * sizeof(char));
    snprintf(name, len, "%%.utmp.%zu", counter);

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
  bool ext_needed = false;
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
    case BOK_AND:
      cmd = "and";
      break;
    case BOK_OR:
      cmd = "or";
      break;
    case BOK_EQ:
      cmd        = "icmp eq";
      ext_needed = true;
      break;
    case BOK_NE:
      cmd        = "icmp ne";
      ext_needed = true;
      break;
    case BOK_LT:
      cmd        = "icmp slt";
      ext_needed = true;
      break;
    case BOK_GT:
      cmd        = "icmp sgt";
      ext_needed = true;
      break;
    case BOK_LE:
      cmd        = "icmp sle";
      ext_needed = true;
      break;
    case BOK_GE:
      cmd        = "icmp sge";
      ext_needed = true;
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
  if (ext_needed) {
    counter++;
    fprintf(file, "%%.tmp.%zu = zext i1 %%.tmp.%zu to i64\n", counter, counter - 1);
  }

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

  fprintf(file, "store i64 %s, ptr %s, align 8\n", rhs_name, metadata->ir_name);

  if (result_name)
    *result_name = rhs_name;
  else
    free((void*) rhs_name);

  return status;
}

static inline int generate_if(FILE* file, ASTNode* node, IdentMetaTable* meta, const char* enclosing_loop_start, const char* enclosing_loop_end) {
  assert(node->kind == NK_IF);

  static size_t counter = 0;
  int status            = 0;

  ASTNode* cond    = node->children;
  ASTNode* iftrue  = cond->next;
  ASTNode* iffalse = iftrue->next;

  size_t local_counter = counter;
  counter++;

  const char* cond_name = NULL;
  status |= generate_expression(file, cond, meta, &cond_name);

  fprintf(file, "%%.iftmp.%zu = icmp ne i64 0, %s\n", local_counter, cond_name);
  fprintf(file, "br i1 %%.iftmp.%zu, label %%.iftrue.%zu, label %%.if%s.%zu\n", local_counter, local_counter, iffalse ? "false" : "end", local_counter);
  fprintf(file, "\n.iftrue.%zu:\n", local_counter);

  status |= generate_statement(file, iftrue, meta, enclosing_loop_start, enclosing_loop_end);
  fprintf(file, "br label %%.ifend.%zu\n", local_counter);


  if (iffalse) {
    fprintf(file, "\n.iffalse.%zu:\n", local_counter);
    status |= generate_statement(file, iffalse, meta, enclosing_loop_start, enclosing_loop_end);
    fprintf(file, "br label %%.ifend.%zu\n", local_counter);
  }

  fprintf(file, "\n.ifend.%zu:\n", local_counter);

  return status;
}

static inline int generate_while(FILE* file, ASTNode* node, IdentMetaTable* meta) {
  assert(node->kind == NK_WHILE);

  static size_t counter = 0;
  int status            = 0;

  size_t local_counter = counter;
  counter++;

  ASTNode* cond = node->children;
  ASTNode* body = cond->next;

  char cond_label[SIZE_T_MAX_DIGITS + sizeof(".whilecond.")];
  char body_label[SIZE_T_MAX_DIGITS + sizeof(".whilebody.")];
  char end_label[SIZE_T_MAX_DIGITS + sizeof(".whileend.")];
  snprintf(cond_label, sizeof(body_label) / sizeof(char), ".whilecond.%zu", local_counter);
  snprintf(body_label, sizeof(body_label) / sizeof(char), ".whilebody.%zu", local_counter);
  snprintf(end_label, sizeof(end_label) / sizeof(char), ".whileend.%zu", local_counter);

  generate_branch(file, cond_label);
  fprintf(file, "\n%s:\n", cond_label);

  const char* cond_name = NULL;
  status |= generate_expression(file, cond, meta, &cond_name);

  fprintf(file, "%%.whiletmp.%zu = icmp ne i64 0, %s\n", local_counter, cond_name);
  fprintf(file, "br i1 %%.whiletmp.%zu, label %%%s, label %%%s\n", local_counter, body_label, end_label);

  fprintf(file, "\n%s:\n", body_label);
  status |= generate_statement(file, body, meta, cond_label, end_label);
  generate_branch(file, cond_label);

  fprintf(file, "\n%s:\n", end_label);

  return status;
}

static inline int generate_block(FILE* file, ASTNode* node, IdentMetaTable* meta, const char* enclosing_loop_start, const char* enclosing_loop_end) {
  assert(node->kind = NK_BLOCK);

  int status     = 0;
  ASTNode** next = &node->children;
  while (*next) {
    status |= generate_statement(file, *next, meta, enclosing_loop_start, enclosing_loop_end);
    next = &((*next)->next);
  }

  return status;
}

static inline int generate_branch(FILE* file, const char* dest) {
  fprintf(file, "br label %%%s\n", dest);
  return 0;
}
