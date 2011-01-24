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
#define ISDIGIT(c)      (c >= '0' && c <= '9')
#define CAR(x)          ((x->data.pair)[0])
#define CDR(x)          ((x->data.pair)[1])
#define CONS(x, y)      (mkpair(x, y))

enum otype { SYM, NUM, PAIR, NIL };
typedef struct Object Object;
struct Object {
    int gc_counter;
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
void freelst(Object *obj);
void printobj(FILE *fp, Object *obj, int cancelout);
int isnum(char *atom);
int islst(Object *obj);
void bindvar(Object **frame, Object *sym, Object *val);
Object *initglobal(void);
Object *extendenv(Object *listofvars, Object *listofvals, Object *env);

int main(void) {
    Object *obj;

    /* (loop (print (eval (read) global))) */
    while (1) {
        print(stdout, eval(obj = read(stdin), NULL));
        freeobj(obj);
    }
    return 0;
}

void setcar(Object *pair, Object *obj) {
    if (pair == NULL || obj == NULL)
        return;
    CAR(pair) = obj;
}

void setcdr(Object *pair, Object *obj) {
    if (pair == NULL || obj == NULL)
        return;
    CDR(pair) = obj;
}

/* 
 * Return a new empty global environment.
 */
Object *initglobal(void) {
    return CONS(mknil(), mknil());
}

/*
 * Bind the symbol to the value, and add the pair to the top level frame of the 
 * environment.
 */
void eval_define(Object *symbol, Object *value, Object *env) {
    Object *frame, *kvpair;

    kvpair = CONS(symbol, value);
    if (kvpair == NULL)
        return;
    setcar(env, CONS(kvpair, CAR(env)));
}

/* 
 * Reads user input and returns the expression as a Lisp list.
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

/*
 * Print the object in a readable and sane format.
 */
void print(FILE *fp, Object *obj) {
    printobj(fp, obj, 0);
    fprintf(stderr, "\n");
}

/*
 * Allocate memory on the heap for a new object. Because this is the lowest level 
 * object creation function, programmers should only directly call the higher order mk* 
 * functions.
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
        CAR(obj) = ((Object **)data)[0];
        CDR(obj) = ((Object **)data)[1];
    } 
    /* If type is NIL, only assign the object a type */
    obj->type = type;
    /* If garbage collector counter > 0 then don't free the
     * object when we're done with it */
    obj->gc_counter = 0;
    return obj;
}

/* 
 * Return a new symbol OBJECT
 */
Object *mksym(char *symbol) {
    return mkobj(symbol, SYM);
}

/* 
 * Return a new number OBJECT
 */
Object *mknum(float f) {
    return mkobj(&f, NUM);
}

/* 
 * Return a new number OBJECT
 */
Object *mknil(void) {
    return mkobj(NULL, NIL);
}

/* 
 * Return a new pair OBJECT. The arguments to this function must not be NULL, so 
 * that when constructing multiple pairs using CONS, they will all fail if one call 
 * returns NULL.
 */
Object *mkpair(Object *car, Object *cdr) {
    Object *pair[2];

    if (car == NULL || cdr == NULL)
        return NULL;
    pair[0] = car;
    pair[1] = cdr;

    return mkobj(pair, PAIR);
}

/*
 * Free the object. If object is a pair, then recursively free the car and cdr 
 * elements and so on.
 */
void freeobj(Object *obj) {
    if (obj == NULL)
        return;
    if (obj->type == SYM) {
        free(obj->data.symbol);
    } else if (obj->type == PAIR) {
        /* Recursively free the pair */
        freeobj(CAR(obj));
        freeobj(CDR(obj));
    }
    free(obj);
}

/* 
 * Free the CONS-PAIR backbone holding the list of elements together. Does NOT free 
 * objects that were pointed to by the list.
 */
void freelst(Object *lst) {
    Object *cdr;
    for (; lst->type == PAIR; lst = cdr) {
        cdr = CDR(lst);
        free(lst);
    }
}

/* 
 * Print a nicely formatted object.
 */
void printobj(FILE *fp, Object *obj, int flag_cancel) {
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
        printobj(CAR(obj), 0);
        /* check next object */
        if (CDR(obj)->type == SYM || CDR(obj)->type == NUM) {
            fprintf(fp, " . ");
            printobj(CDR(obj), 0);
            fprintf(fp, ")");
        } else {
            if (CDR(obj)->type != NIL)
                fprintf(fp, " ");
            printobj(CDR(obj), 1);
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
 * can access using CAR and CDR.
 */
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
            obj = CONS(mksym("quote"), CONS(obj, mknil()));
        }
        expr = CONS(obj, expr);
    }
    while (!stack_isempty(s) && strcmp((char *)stack_top(s), "\'") == 0) {
        free(stack_pop(s));
        expr = CONS(mksym("quote"), CONS(expr, mknil()));
    }
    return expr;
}

/* 
 * Read from file. Parentheses matching error checking happens here. Result is returned 
 * as a stack of tokens which the programmer should then pass to parsetoks.
 */
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

/* 
 * Simple boolean that returns non-zero if the string is real number.
 */
int isnum(char *atom) {
    int decimalcount = 0;

    if (*atom == '-')
        atom++;
    for (; *atom != '\0'; atom++) {
        if (*atom == '.') {
            if (++decimalcount > 1)
                break;
        } else if (!ISDIGIT(*atom)) {
            break;
        }
    }
    /* check if we reached the end */
    return (*atom == '\0') ? 1 : 0;
}
