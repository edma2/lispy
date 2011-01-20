/* lisp.c - minimal lisp interpreter 
 * author: Eugene Ma (edma2)
 * TODO: environments, lambda procs, numerical tower?
 * pair == 2 item array?
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "stack.h"

#define MAXATOM         100
#define MAXLINE         1000
#define isdigit(c)      (c >= '0' && c <= '9')
#define car(x)          ((x->data.pair)[0])
#define cdr(x)          ((x->data.pair)[1])
#define cons(x, y)      (mkpair(x, y))

enum otype { SYM, NUM, PAIR, NIL };
typedef struct Object Object;
struct Object {
    union {
        char *symbol;
        float number;
        Object *pair[2];
    } data;
    enum otype type;
};

Object *read(FILE *fp);
Object *eval(Object *obj, Object *env); //todo
void print(Object *obj);
Object *mkobj(void *data, enum otype type);
Object *mknum(float f);
Object *mksym(char *symbol);
Object *mkpair(Object *car, Object *cdr);
Object *mknil(void);
Stack *gettoks(FILE *fp);
Object *parsetoks(Stack **s);
void freeobj(Object *obj);
void printobj(Object *obj, int cancelout);
int isnum(char *atom);
Object *mkvar(Object *key, Object *val);
void bindvar(Object **frame, Object *sym, Object *val);

int main(void) {
    Object *obj;

    /* (loop (print (eval (read) global))) */
    while (1) {
        print(eval(obj = read(stdin), NULL));
        freeobj(obj);
    }
    return 0;
}

/*
 * Environments 
 *
 */
/* XXX: setup the environment here */

Object *mkvar(Object *key, Object *val) {
    if (key->type != SYM)
        return NULL;
    return cons(key, val);
}

/* Bind a variable to the frame */
void bindvar(Object **frame, Object *sym, Object *val) {
    Object *bind;

    bind = mkvar(sym, val);
    if (bind == NULL)
        return;
    *frame = cons(bind, *frame);
}

Object *assoc(Object *key, Object *alist) {
    return NULL;
}

/*
 * Core
 *
 */

Object *read(FILE *fp) {
    Stack *s;

    printf("> ");
    s = gettoks(fp);

    return parsetoks(&s);
}

Object *eval(Object *obj, Object *env) {
    return obj;
}

void print(Object *obj) {
    printobj(obj, 0);
    fprintf(stderr, "\n");
}

/*
 * Constructors
 *
 */

Object *mkobj(void *data, enum otype type) {
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
    /* if type is NIL, only assign the object a type */
    obj->type = type;
    return obj;
}

Object *mksym(char *symbol) {
    return mkobj(symbol, SYM);
}

Object *mknum(float f) {
    return mkobj(&f, NUM);
}

Object *mknil(void) {
    return mkobj(NULL, NIL);
}

Object *mkpair(Object *car, Object *cdr) {
    Object *pair[2];
    pair[0] = car;
    pair[1] = cdr;
    return mkobj(pair, PAIR);
}

/*
 * Clean Up
 *
 */

void freeobj(Object *obj) {
    if (obj == NULL)
        return;
    if (obj->type == SYM) {
        free(obj->data.symbol);
    } else if (obj->type == PAIR) {
        /* Recursively free the pair */
        freeobj(car(obj));
        freeobj(cdr(obj));
    }
    free(obj);
}

/* 
 * Debugging
 *
 */

void printobj(Object *obj, int flag_cancel) {
    if (obj == NULL) {
        fprintf(stderr, "print: missing object");
    } else if (obj->type == NUM) {
        /* XXX: remove trailing zeroes */
        fprintf(stderr, "%f", obj->data.number);
    } else if (obj->type == SYM) {
        fprintf(stderr, "%s", obj->data.symbol);
    } else if (obj->type == PAIR) {
        if (flag_cancel == 0)
            fprintf(stderr, "(");
        printobj(car(obj), 0);
        /* check next object */
        if (cdr(obj)->type == SYM || cdr(obj)->type == NUM) {
            fprintf(stderr, " . ");
            printobj(cdr(obj), 0);
            fprintf(stderr, ")");
        } else {
            if (cdr(obj)->type != NIL)
                fprintf(stderr, " ");
            printobj(cdr(obj), 1);
        }
    } else if (obj->type == NIL) {
        if (flag_cancel == 0)
            fprintf(stderr, "(");
        fprintf(stderr, ")");
    } else {
        fprintf(stderr, "error: unknown object type\n");
    }
}

/* 
 * S-Expression parser 
 *
 */

/* Parse the tokens returned by gettoks() */
Object *parsetoks(Stack **s) {
    Object *obj, *expr = NULL;
    char *tok;

    /* fetch tokens from stack */
    while (!stack_isempty(s)) {
        tok = stack_pop(s);
        /* examine token */
        if (strcmp(tok, ")") == 0) {
            if (expr == NULL) {
                free(tok);
                expr = mknil();
                continue;
            } else {
                /* put it back on the stack and recurse */
                stack_push(s, tok);
                obj = parsetoks(s);
            }
        } else if (strcmp(tok, "(") == 0) {
            free(tok);
            break;
        } else if (*tok && strcmp(tok, "\'") != 0) {
            obj = isnum(tok) ? mknum(atof(tok)) : mksym(tok);
            free(tok);
            if (obj == NULL)
                break;
            /* symbol atom */
            if (expr == NULL) {
                expr = obj;
                break;
            }
        }
        /* check the top for quote */
        while (!stack_isempty(s) && strcmp((char *)stack_top(s), "\'") == 0) {
            /* pop it off */
            free(stack_pop(s));
            obj = cons(mksym("quote"), cons(obj, mknil()));
        }
        expr = cons(obj, expr);
    }
    while (!stack_isempty(s) && strcmp((char *)stack_top(s), "\'") == 0) {
        free(stack_pop(s));
        expr = cons(mksym("quote"), cons(expr, mknil()));
    }
    return expr;
}

/* READ from file. Error checking happens here.
 * Result returned as a stack of tokens */
Stack *gettoks(FILE *fp) {
    Stack *s = stack_new();
    char atom[MAXATOM];
    char buf[MAXLINE];
    char *str;
    int i, k;
    int depth = 0;

    do {
        if (fgets(buf, MAXLINE, fp) == NULL) {
            while (!stack_isempty(&s))
                free(stack_pop(&s));
            return NULL;
        }
        for (k = 0; k < strlen(buf);) {
            if (buf[k] ==  '(' || buf[k] == ')' || buf[k] == '\'') {
                if (buf[k] == '(') {
                    ++depth;
                } else if (buf[k] == ')') {
                    /* break condition 1: too many close parens */
                    if (--depth < 0) {
                        fprintf(stderr, "lisp: unexpected close parens\n");
                        break;
                    }
                }
                snprintf(atom, MAXATOM, "%c", buf[k++]);
                str = strdup(atom);
                if (str == NULL) {
                    /* break condition 2: memory allocation error */
                    fprintf(stderr, "error: malloc() failed\n");
                    break;
                }
                stack_push(&s, str);
            } else if (buf[k] != ' ' && buf[k] != '\n') {
                for (i = 0; i < MAXATOM-1; i++, k++) {
                    if (buf[k] == '\n' || buf[k] == ' ' || buf[k] == '(' || buf[k] == ')')
                        break;
                    atom[i] = buf[k];
                }
                atom[i] = '\0';
                str = strdup(atom);
                if (str == NULL) {
                    fprintf(stderr, "error: malloc() failed\n");
                    break;
                }
                stack_push(&s, str);
            }
            /* eat whitespace */
            while (buf[k] == ' ' || buf[k] == '\n')
                k++;
        }
        if (buf[k] != '\0') {
            while (!stack_isempty(&s))
                free(stack_pop(&s));
            return NULL;
        }
    } while (depth != 0);

    return s;
}

/* Return non-zero if the string is real number */
int isnum(char *atom) {
    int decimalcount = 0;

    if (*atom == '-')
        atom++;
    for (; *atom != '\0'; atom++) {
        if (*atom == '.') {
            if (++decimalcount > 1)
                break;
        } else if (!isdigit(*atom)) {
            break;
        }
    }
    /* check if we reached the end */
    return (*atom == '\0') ? 1 : 0;
}
