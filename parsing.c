#include <stdio.h>
#include <stdlib.h>

#include "mpc.h"

#ifdef _WIN32
#include <string.h>
static char buffer[2048];

char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cpy = malloc(strlen(buffer)+1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

void add_history(char* unused) {}

#else

#include <editline/readline.h>
#include <editline/history.h>

#endif


typedef struct lval {
  char* err;
  char* sym;
  double num;
  int count;
  int type;
  struct lval** cell;
} lval;

enum { LVAL_ERR, LVAL_NUM, LVAL_SEXPR, LVAL_SYM };

enum { LERR_UNKNOWN, LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM, LERR_BAD_ARGS };

lval* lval_err(char* err);
lval* lval_num(double x);
lval* lval_sexpr(void);
lval* lval_sym(char* sym);

lval* lval_add(lval* v, lval* x);
void lval_del(lval* v);

lval* lval_read(mpc_ast_t* t);
lval* lval_read_number(mpc_ast_t* number_node);
lval* lval_read_sexpr(mpc_ast_t* t);
lval* lval_read_symbol(mpc_ast_t* t);

void lval_expr_print(lval* v, char open, char close);
void lval_print(lval* v);
void lval_println(lval* v);

lval* lval_err(char* err) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->err = malloc(strlen(err) + 1);
  strcpy(v->err, err);
  return v;
}

lval* lval_num(double x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

lval* lval_sexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval* lval_sym(char* sym) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(sym) + 1);
  strcpy(v->sym, sym);
  return v;
}

lval* lval_add(lval* v, lval* x) {
  // add x to the children of v
  puts("adding x to v");
  lval_println(x);
  lval_println(v);
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count-1] = x;
  return v;
}

void lval_del(lval* v) {
  switch(v->type) {
    case LVAL_NUM: break;
    case LVAL_ERR:free(v->err); break;
    case LVAL_SYM:free(v->sym); break;
    case LVAL_SEXPR:
      for (int i = 0; i < v->count; i++) {
        lval_del(v->cell[i]);
      }
      free(v->cell);
      break;
  }
  free(v);
}

lval* lval_read(mpc_ast_t* t) {
  // symbols and numbers can be directly converted to that type
  if (strstr(t->tag, "number")) { return lval_read_number(t); }
  if (strstr(t->tag, "symbol")) { return lval_read_symbol(t); }
  if (strcmp(t->tag, ">") == 0) { return lval_read_sexpr(t); } // root node
  if (strstr(t->tag, "sexpr"))  { return lval_read_sexpr(t); }
  return lval_err("Unexpected node");
}

lval* lval_read_number(mpc_ast_t* number_node) {
  errno = 0; // I think errno is written to by strtof if there's a problem

  // integer case
  if (strstr(number_node->tag, "int")) {
    double x = (double)strtof(number_node->contents, NULL);
    return (errno == ERANGE) ? lval_err("invalid number") : lval_num(x);
  }

  // decimal case
  // This node has between 3 and 4 children.
  char* temp = malloc(2048);
  strcpy(temp, number_node->children[0]->contents);
  for (int i = 1; i < number_node->children_num; i++) {
    strcat(temp, number_node->children[i]->contents);
  }
  double result = strtof(temp, NULL);
  free(temp);
  return (errno == ERANGE) ? lval_err("invalid number") : lval_num(result);
}

lval* lval_read_sexpr(mpc_ast_t* t) {
  lval* x = lval_sexpr();
  for (int i=0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }

    x = lval_add(x, lval_read(t->children[i]));
  }
  return x;
}

lval* lval_read_symbol(mpc_ast_t* t) {
  return lval_sym(t->contents);
}

void lval_expr_print(lval* v, char open, char close) {
  putchar(open);
  for (int i=0; i < v->count; i++) {
    lval_print(v->cell[i]);
    
    if (i != (v->count-1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

void lval_print(lval* v) {
  switch (v->type) {
    case LVAL_NUM: printf("%f", v->num); break;
    case LVAL_ERR: printf("Error: %s", v->err); break;
    case LVAL_SYM: printf("%s", v->sym); break;
    case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
  }
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }

lval eval_expr(mpc_ast_t* node);

/*
lval op_add(int count, mpc_ast_t** nodes) {
  lval x = eval_expr(nodes[0]);
  if (x.type == LVAL_ERR) { return x; }

  for (int i=1; i<count; i++) {
    lval y = eval_expr(nodes[i]);
    if (y.type == LVAL_ERR) { return y; }

    x = lval_num(x.num + y.num);
  }
  return x;
}

lval op_sub(int count, mpc_ast_t** nodes) {
  lval x = eval_expr(nodes[0]);
  if (x.type == LVAL_ERR) { return x; }

  if (count==1) { return lval_num(0 - x.num); }


  for (int i=1; i<count; i++) {
    lval y = eval_expr(nodes[i]);
    if (y.type == LVAL_ERR) { return y; }

    x = lval_num(x.num - y.num);
  }
  return x;
}

lval op_mul(int count, mpc_ast_t** nodes) {
  if (count==1) { return lval_err(LERR_BAD_ARGS); }

  lval x = eval_expr(nodes[0]);
  if (x.type == LVAL_ERR) { return x; }

  for (int i=1; i<count; i++) {
    lval y = eval_expr(nodes[i]);
    if (y.type == LVAL_ERR) { return y; }

    x = lval_num(x.num * y.num);
  }
  return x;
}

lval op_div(int count, mpc_ast_t** nodes) {
  // TODO: handle lval_err args everywhere
  if (count==1) { return lval_err(LERR_BAD_ARGS); }

  lval x = eval_expr(nodes[0]);
  if (x.type == LVAL_ERR) { return x; }

  for (int i=1; i<count; i++) {
    lval y = eval_expr(nodes[i]);
    if (y.type == LVAL_ERR) { return x; }
    if (y.num == 0) { return lval_err(LERR_DIV_ZERO); }

    x = lval_num(x.num / y.num);
  }
  return x;
}

lval op_pow(int count, mpc_ast_t** nodes) {
  // TODO: Convert these to return bad args
  if (count==1) { return lval_err(LERR_BAD_ARGS); }

  lval x = eval_expr(nodes[0]);
  if (x.type == LVAL_ERR) { return x; }

  for (int i=1; i<count; i++) {
    lval y = eval_expr(nodes[i]);
    if (y.type == LVAL_ERR) { return y; }

    x = lval_num(pow(x.num, y.num));
  }
  return x;
}

double min(double x, double y) {
  return (((x) < (y)) ? (x) : (y));
}

lval op_min(int count, mpc_ast_t** nodes) {
  lval x = eval_expr(nodes[0]);
  if (x.type == LVAL_ERR) { return x; }

  for (int i=1; i<count; i++) {
    lval y = eval_expr(nodes[i]);
    if (y.type == LVAL_ERR) { return y; }

    x = lval_num(min(x.num, y.num));
  }
  return x;
}

double max(double x, double y) {
  return (((x) > (y)) ? (x) : (y));
}

lval op_max(int count, mpc_ast_t** nodes) {
  lval x = eval_expr(nodes[0]);
  if (x.type == LVAL_ERR) { return x; }

  for (int i=1; i<count; i++) {
    lval y = eval_expr(nodes[i]);
    if (y.type == LVAL_ERR) { return y; }

    x = lval_num(max(x.num, y.num));
  }
  return x;
}

lval eval_op(mpc_ast_t* node) {
  // TODO: something to do here for lval?
  // Parse nodes are regex, symbol, arguments..., regex
  char* op = node->children[1]->contents;
  mpc_ast_t** tail = node->children+2;
  int children_num = node->children_num - 3; // don't include first, second, last

  if ( strcmp(op, "+") == 0 || strcmp(op, "add") == 0) {
    return op_add(children_num, tail);
  }
  if ( strcmp(op, "-") == 0 || strcmp(op, "sub") == 0) {
    return op_sub(children_num, tail);
  }
  if ( strcmp(op, "*") == 0 || strcmp(op, "mul") == 0) {
    return op_mul(children_num, tail);
  }
  if ( strcmp(op, "/") == 0 || strcmp(op, "div") == 0) {
    return op_div(children_num, tail);
  }
  if ( strcmp(op, "^") == 0 || strcmp(op, "pow") == 0) {
    return op_pow(children_num, tail);
  }
  if ( strcmp(op, "min") == 0 ) {
    return op_min(children_num, tail);
  }
  if ( strcmp(op, "max") == 0 ) {
    return op_max(children_num, tail);
  }

  return lval_err(LERR_BAD_OP);
}

lval eval_expr(mpc_ast_t* node) {
  // number
  if (strstr(node->tag, "number")) {
    return eval_number(node);
  }

  if (strstr(node->tag, ">")) {
    return eval_op(node);
  }

  printf("ERROR: unexpected parse node\n");
  return lval_err(LERR_UNKNOWN);
}

*/
int main(int argc, char** argv) {
  puts("actually evaluate the sexpr next?");
  // Define parsers
  mpc_parser_t* Int =    mpc_new("int");
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* Sexpr =  mpc_new("sexpr");
  mpc_parser_t* Expr =   mpc_new("expr");
  mpc_parser_t* Repl =   mpc_new("repl");

  // grammar
  mpca_lang(
      MPCA_LANG_DEFAULT,
      "                                      \
      int : '0' | /[1-9][0-9]*/ ;            \
                                             \
      number : '-'? <int> ('.' /[0-9]+/)?;   \
                                             \
      symbol :                               \
       '+'                                   \
      | '-'                                  \
      | '*'                                  \
      | '/'                                  \
      | '^'                                  \
      | \"add\"                              \
      | \"sub\"                              \
      | \"mul\"                              \
      | \"div\"                              \
      | \"pow\"                              \
      | \"min\"                              \
      | \"max\"                              \
      ;                                      \
                                             \
      sexpr : '(' <expr>* ')' ;              \
                                             \
      expr : <number> | <symbol> | <sexpr> ; \
                                             \
      repl : /^/ <expr>* /$/ ;               \
      ",
      Int,
      Number,
      Symbol,
      Sexpr,
      Expr,
      Repl
      );

  puts("Hovis REPL Version 0.1");
  puts("Press Ctrl+c to exit\n");

  // main loop
  while (1) {
    char* input = readline("repl> ");

    // parse repl input
    mpc_result_t parse_result;
    if (mpc_parse("<stdin>", input, Repl, &parse_result)) {
      // parse success
      mpc_ast_print(parse_result.output);

      lval* root = lval_read(parse_result.output);
      lval_println(root);
      lval_del(root);

      mpc_ast_delete(parse_result.output);
    } else {
      mpc_err_print(parse_result.error);
      mpc_err_delete(parse_result.error);
    }
    add_history(input);
    free(input);
  }

  return 0;

  // Clean up parsers
  mpc_cleanup(5, Int, Number, Symbol, Sexpr, Expr, Repl);
}


