/* 
 * Minimal LISP interpreter - lisp.c
 * Written by Eugene Ma (edma2)
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "stack.h"

#define MAXATOM         100
#define MAXLINE         1000
#define is_digit(c)     (c >= '0' && c <= '9')
#define is_space(c)     (c == ' ' || c == '\n')
#define car(x)          ((x->data.pair)[0])
#define cdr(x)          ((x->data.pair)[1])
#define cons(x, y)      (make_pair(x, y))

enum otype { SYM, NUM, PAIR, NIL };
typedef struct Object Object;
struct Object {
        union {
                char *symbol;
                float number;
                Object *pair[2];
        } data;
        enum otype type;
        int refcount;
};

Object *make_obj(void *data, enum otype type);
Object *make_number(float f);
Object *make_symbol(char *symbol);
Object *make_pair(Object *car, Object *cdr);
Object *make_nil(void);
Object *read(FILE *fp);
Object *parse_tokens(Stack **s);
Stack *get_tokens(FILE *fp);
char *get_token(FILE *fp);
Stack *push_tokens(FILE *fp);
void print(FILE *fp, Object *obj);
void print_obj(FILE *fp, Object *obj, int cancelout);
void free_obj(Object *obj);
int is_number(char *atom);

int main(void) {
        Object *obj;

        /* (loop (print (eval (read) global))) */
        while (1) {
                obj = read(stdin);
                print(stdout, obj);
                free_obj(obj);
        }
        return 0;
}

/* 
 * Reads user input and returns the expression as a Lisp list.
 */
Object *read(FILE *fp) {
        Stack *s;

        printf("> ");
        s = push_tokens(fp);

        return parse_tokens(&s);
}

/*
 * Print the object in a readable and sane format.
 */
void print(FILE *fp, Object *obj) {
        print_obj(fp, obj, 0);
        fprintf(fp, "\n");
}

/*
 * Allocate memory on the heap for a new object. Because this is the lowest level 
 * object creation function, programmers should only directly call the higher order mk* 
 * functions.
 */
Object *make_obj(void *data, enum otype type) {
        Object *obj;

        obj = malloc(sizeof(Object));
        if (obj == NULL)
                return NULL;
        if (type == SYM) {
                obj->data.symbol = strdup((char *)data);
                /* Check that strdup succeeded */
                if (obj->data.symbol == NULL) {
                        fprintf(stderr, "error: malloc() failed\n");
                        free(obj);
                        return NULL;
                }
        } else if (type == NUM) {
                obj->data.number = *(float *)data; 
        } else if (type == PAIR) {
                car(obj) = ((Object **)data)[0];
                cdr(obj) = ((Object **)data)[1];
        } 
        /* If type is NIL, only assign the object a type */
        obj->type = type;
        /* Initialize all reference counts to 0 */
        obj->refcount = 0;
        return obj;
}

/* 
 * Return a new symbol OBJECT
 */
Object *make_symbol(char *symbol) {
        return make_obj(symbol, SYM);
}

/* 
 * Return a new number OBJECT
 */
Object *make_number(float f) {
        return make_obj(&f, NUM);
}

/* 
 * Return a new empty list
 */
Object *make_nil(void) {
        return make_obj(NULL, NIL);
}

/* 
 * Return a new pair OBJECT. The arguments to this function must not be NULL, so 
 * that when constructing multiple pairs using CONS, they will all fail if one call 
 * returns NULL.
 */
Object *make_pair(Object *car, Object *cdr) {
        Object *pair[2];

        if (car == NULL || cdr == NULL)
                return NULL;
        pair[0] = car;
        pair[1] = cdr;

        return make_obj(pair, PAIR);
}

/*
 * Free the object. If object is a pair, then recursively free the car and cdr 
 * elements and so on.
 */
void free_obj(Object *obj) {
        if (obj == NULL)
                return;
        if (obj->type == SYM) {
                free(obj->data.symbol);
        } else if (obj->type == PAIR) {
                /* Recursively free the pair */
                free_obj(car(obj));
                free_obj(cdr(obj));
        }
        free(obj);
}

/* 
 * Print a nicely formatted object.
 */
void print_obj(FILE *fp, Object *obj, int flag_cancel) {
        if (obj == NULL) {
                fprintf(stderr, "print: missing object");
        } else if (obj->type == NUM) {
                /* XXX: remove trailing zeroes */
                fprintf(fp, "%f", obj->data.number);
        } else if (obj->type == SYM) {
                fprintf(fp, "%s", obj->data.symbol);
        } else if (obj->type == PAIR) {
                if (flag_cancel == 0)
                        fprintf(fp, "(");
                print_obj(fp, car(obj), 0);
                /* check next object */
                if (cdr(obj)->type == SYM || cdr(obj)->type == NUM) {
                        fprintf(fp, " . ");
                        print_obj(fp, cdr(obj), 0);
                        fprintf(fp, ")");
                } else {
                        if (cdr(obj)->type != NIL)
                                fprintf(fp, " ");
                        print_obj(fp, cdr(obj), 1);
                }
        } else if (obj->type == NIL) {
                if (flag_cancel == 0)
                        fprintf(fp, "(");
                fprintf(fp, ")");
        } else {
                fprintf(fp, "error: unknown object type\n");
        }
}

/* 
 * Given a stack of tokens returned by gettok, turn it into a Lisp list we 
 * can access using car and cdr.
 */
Object *parse_tokens(Stack **s) {
        Object *obj, *expr = NULL;
        char *tok;

        /* Fetch tokens from stack */
        while (*s != NULL) {
                /* Fetch token */
                tok = stack_pop(s);
                /* 
                 * If token is a close paren, then call this function to build the 
                 * expression recursively until we see an open paren.
                 */
                if (tok[0] == ')') {
                        /* Last item of list is always an empty list */
                        if (expr == NULL) {
                                free(tok);
                                expr = make_nil();
                                continue;
                        } else {
                                stack_push(s, tok);
                                obj = parse_tokens(s);
                        }
                } else if (tok[0] == '(') {
                        free(tok);
                        break;
                } else if (tok[0] != '\0' && tok[0] != '\'') {
                        obj = is_number(tok) ? make_number(atof(tok)) : make_symbol(tok);
                        free(tok);
                        if (obj == NULL)
                                break;
                        if (expr == NULL) {
                                expr = obj;
                                break;
                        }
                }
                /* Take care of inner quotes */
                while (*s != NULL && ((char *)stack_top(s))[0] == '\'') {
                        free(stack_pop(s));
                        obj = cons(make_symbol("quote"), cons(obj, make_nil()));
                }
                expr = cons(obj, expr);
        }
        /* Take care of outer quote */
        while (*s != NULL && ((char *)stack_top(s))[0] == '\'') {
                free(stack_pop(s));
                expr = cons(make_symbol("quote"), cons(expr, make_nil()));
        }
        return expr;
}

/* 
 * Read the next token from file. 
 */ 
char *get_token(FILE *fp) {
        char tok[MAXATOM];
        char c;
        int i;
        
        /* Eat up whitespace */
        do {
                if ((c = getc(fp)) == EOF)
                        return NULL;
        } while (is_space(c));
        /* Stop at first non-whitespace char */
        if (strchr("()\'", c) != NULL) {
                tok[0] = c;
                tok[1] = '\0';
        } else {
                for (i = 0; i < MAXATOM; i++) {
                        /* Stop at first whitespace or special char */
                        if (is_space(c) || strchr("()\'", c) != NULL) {
                                ungetc(c, fp);
                                tok[i] = '\0';
                                break;
                        }
                        tok[i] = c;
                        if ((c = getc(fp)) == EOF)
                                return NULL;
                }
        }
        return strdup(tok);
}

/* 
 * Push tokens into a stack 
 */
Stack *push_tokens(FILE *fp) {
        Stack *s = NULL;
        char *tok;
        int depth = 0;

        do {
                /* If error free stack and return NULL */
                if ((tok = get_token(fp)) == NULL) {
                        while (s != NULL)
                                free(stack_pop(&s));
                        break;
                }
                if (tok[0] == '(') {
                        ++depth;
                } else if (tok[0] == ')') {
                        if (--depth < 0) {
                                fprintf(stderr, "error: unexpected ')'\n");
                                free(tok);
                                break;
                        }
                }
                stack_push(&s, tok);
        } while (tok[0] == '\'' || depth > 0);
        return s;
}

/* 
 * Simple boolean that returns non-zero if the string is real number.
 */
int is_number(char *atom) {
        for (; is_digit(*atom); atom++);
        if (*atom == '.')
                for (atom++; is_digit(*atom); atom++);
        return (*atom == '\0');
}
