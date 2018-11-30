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


typedef struct {
  int type;
  double num;
  int err;
} lval;

enum { LVAL_NUM, LVAL_ERR };

enum { LERR_UNKNOWN, LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM, LERR_BAD_ARGS };

lval lval_num(double x) {
  lval v;
  v.type = LVAL_NUM;
  v.num = x;
  return v;
}

lval lval_err(int x) {
  lval v;
  v.type = LVAL_ERR;
  v.err = x;
  return v;
}

void lval_print(lval v) {
  switch (v.type) {
    case LVAL_NUM: printf("%f", v.num); break;
    case LVAL_ERR:
      if (v.err == LERR_UNKNOWN) {
        printf("Error: Unknown error");
      }
      if (v.err == LERR_DIV_ZERO) {
        printf("Error: Div zero");
      }
      if (v.err == LERR_BAD_OP) {
        printf("Error: Invalid operator");
      }
      if (v.err == LERR_BAD_NUM) {
        printf("Error: Invalid number");
      }
      if (v.err == LERR_BAD_ARGS) {
        printf("Error: Invalid arguments");
      }
      break;
  }
}

void lval_println(lval v) { lval_print(v); putchar('\n'); }

lval eval_expr(mpc_ast_t* node);

int number_of_nodes(mpc_ast_t* node) {
  // base case
  if (node->children_num == 0) { return 1; }

  // recursive case
  int total = 1;
  for (int i = 0; i < node->children_num; i++) {
    total += number_of_nodes(node->children[i]);
  }
  return total;
}

lval eval_number(mpc_ast_t* number_node) {
  // integer case
  if (strstr(number_node->tag, "int")) { return lval_num((double)atof(number_node->contents)); }

  // decimal case
  // This node has between 3 and 4 children.
  char* temp = malloc(2048);
  strcpy(temp, number_node->children[0]->contents);
  int i = 1;
  while (i < number_node->children_num) {
    strcat(temp, number_node->children[i]->contents);
    i++;
  }
  lval result = lval_num(atof(temp));
  free(temp);
  return result;
}

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
  // Parse nodes are regex, operator, arguments..., regex
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

int main(int argc, char** argv) {
  // Define parsers
  mpc_parser_t* Int = mpc_new("int");
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Operator = mpc_new("operator");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Repl = mpc_new("repl");

  // grammar
  mpca_lang(
      MPCA_LANG_DEFAULT,
      "                                    \
      int :                                \
        '0'                                \
      | /[1-9][0-9]*/                      \
      ;                                    \
                                           \
      number :                             \
        '-'? <int> ('.' /[0-9]+/)?;        \
                                           \
      operator :                           \
       '+'                                 \
      | '-'                                \
      | '*'                                \
      | '/'                                \
      | '^'                                \
      | \"add\"                            \
      | \"sub\"                            \
      | \"mul\"                            \
      | \"div\"                            \
      | \"pow\"                            \
      | \"min\"                            \
      | \"max\"                            \
      ;                                    \
                                           \
      expr :                               \
        <number>                           \
      | '(' <operator> <expr>+ ')'         \
      ;                                    \
                                           \
      repl :                               \
        /^/ <operator> <expr>+ /$/         \
      ;                                    \
                                           \
      ",
      Int,
      Number,
      Operator,
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

      mpc_ast_t* parse_root = parse_result.output;
      lval result = eval_expr(parse_root);
      lval_println(result);

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
  mpc_cleanup(5, Int, Number, Operator, Expr, Repl);
}


