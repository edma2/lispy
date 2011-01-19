/* lisp.c - minimal lisp interpreter 
 * author: Eugene Ma (edma2)
 * TODO: environments, lambda procs, numerical tower?
 * pair == 2 item array?
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "stack.h"

#define ATOMLEN         100
#define READLEN         1000
#define isdigit(c)      (c >= '0' && c <= '9')
#define car(x)          ((x->data.pair)[0])
#define cdr(x)          ((x->data.pair)[1])
#define cons(x, y)      (mkpair(x, y))

/* Typing */
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

Object *read(FILE *f);
Object *eval(Object *obj, Object *env); //todo
void print(Object *obj);
Object *mkobj(void *data, enum otype type);
Object *mknum(float f);
Object *mksym(char *symbol);
Object *mkpair(Object *car, Object *cdr);
Object *mknil(void);
Stack *gettoks(char *buf);
Object *parsetoks(Stack **s);
void freeobj(Object *obj);
void printobj(Object *obj, int cancelout);
int isnum(char *atom);

int main(void) {
    Object *obj;

    /* (loop (print (eval (read) global))) */
    while (1) {
        print(eval(obj = read(stdin), NULL));
        freeobj(obj);
    }

    return 0;
}

Object *read(FILE *f) {
    char buf[READLEN];
    Stack *s;

    printf("> ");
    if (fgets(buf, READLEN, f) == NULL)
        return NULL;
    /* remove newline */
    buf[strlen(buf) - 1] = '\0';
    s = gettoks(buf);
    if (s == NULL)
        fprintf(stderr, "read: mismatched parens\n");
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
        } else if (strcmp(tok, "\'") != 0) {
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

/* Push all the tokens in to a stack and return it */
Stack *gettoks(char *buf) {
    Stack *s = stack_new();
    char atom[ATOMLEN];
    char *str;
    int i;
    int depth = 0;

    while (*buf != '\0') {
        if (*buf ==  '(' || *buf == ')' || *buf == '\'') {
            if (*buf == '(')
                depth++;
            else if (*buf == ')')
                depth--;
            snprintf(atom, ATOMLEN, "%c", *buf++);
            str = strdup(atom);
            if (str == NULL) {
                fprintf(stderr, "error: malloc() failed\n");
                break;
            }
            stack_push(&s, str);
        } else if (*buf != ' ' && *buf != '\n') {
            for (i = 0; i < ATOMLEN-1; i++) {
                if (!*buf || *buf == '\n' || *buf == ' ' || *buf == '(' || *buf == ')')
                    break;
                atom[i] = *buf++;
            }
            atom[i] = '\0';
            str = strdup(atom);
            if (str == NULL) {
                fprintf(stderr, "error: malloc() failed\n");
                break;
            }
            stack_push(&s, str);
        } else {
            /* eat whitespace */
            while (*buf == ' ' || *buf == '\n')
                buf++;
        }
    }
    /* clean up */
    if (*buf != '\0' || depth != 0) {
        while (!stack_isempty(&s))
            free(stack_pop(&s));
        return NULL;
    }
    return s;
}

/* Return non-zero if the string is real number */
int isnum(char *atom) {
    int decimalcount = 0;

    if (*atom == '-')
        atom++;
    if (*atom == '\0')
        return 0;
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
