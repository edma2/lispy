/* lisp.c - minimal lisp interpreter 
 * author: Eugene Ma (edma2) */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "stack.h"

#define MAXLEN         100

/* Typing */
enum otype { SYM, NUM, PAIR, NIL };
typedef struct Pair Pair;
typedef struct {
        union {
                char *symbol;
                float number; 
                Pair *pair;
        } data;
        enum otype type;
} Object;
struct Pair {
        Object *car;
        Object *cdr;
};

Object *car(Object *obj);
Object *cdr(Object *obj);
Object *cons(Object *car, Object *cdr);
void setCar(Object *obj, Object *car);
void setCdr(Object *obj, Object *cdr);
Object *newObj(void *data, enum otype type);
Object *newNum(float *f);
Object *newSym(char *symbol);
Object *newNil(void);
Stack *getTokens(char *buf);
Object *parseTokens(Stack **s);
void freeObj(Object *obj);
void printObj(Object *obj);
void printObj_newline(Object *obj);
int isNum(char *atom);
int isDigit(char c);

int main(void) {
        Stack *s;
        Object *obj;

        s = getTokens("'(1 2 3)");
        obj = parseTokens(&s);
        printObj_newline(obj);
        freeObj(obj);

        return 0;
}

/*
 * Mutators
 *
 */

void setCar(Object *obj, Object *car) {
        if (obj == NULL)
                return;
        obj->data.pair->car = car;
}

void setCdr(Object *obj, Object *cdr) {
        if (obj == NULL)
                return;
        obj->data.pair->cdr = cdr;
}

/*
 * Accessors
 *
 */

Object *car(Object *obj) {
        if (obj == NULL)
                return NULL;
        if (obj->type != PAIR)
                return NULL;
        return obj->data.pair->car;
}

Object *cdr(Object *obj) {
        if (obj == NULL)
                return NULL;
        if (obj->type != PAIR)
                return NULL;
        return obj->data.pair->cdr;
}

/*
 * Constructors
 *
 */

Object *newObj(void *data, enum otype type) {
        Object *obj;

        obj = malloc(sizeof(Object));
        if (obj == NULL)
                return NULL;

        if (type == SYM) {
                obj->data.symbol = strdup((char *)data);
                /* Check that strdup succeeded */
                if (obj->data.symbol == NULL) {
                        free(obj);
                        return NULL;
                }
        } else if (type == NUM) {
                obj->data.number = *(float *)data; 
        } else if (type == PAIR) {
                obj->data.pair = (Pair *)data;
        } 
        /* if type is NIL, only assign the object a type */
        obj->type = type;

        return obj;
}

Object *newSym(char *symbol) {
        return newObj(symbol, SYM);
}

Object *newNum(float *f) {
        return newObj(f, NUM);
}

Object *newNil(void) {
        return newObj(NULL, NIL);
}

/* Returns a new PAIR created by the cons procedure */
Object *cons(Object *car, Object *cdr) {
        Pair *p;
        Object *obj;

        p = malloc(sizeof(Pair));
        if (p == NULL)
                return NULL;
        p->car = car;
        p->cdr = cdr;
        obj = newObj(p, PAIR);
        if (obj == NULL) {
                free(p);
                return NULL;
        }

        return obj;
}

/*
 * Clean Up
 *
 */

void freeObj(Object *obj) {
        if (obj == NULL)
                return;
        if (obj->type == SYM) {
                free(obj->data.symbol);
        } else if (obj->type == PAIR) {
                /* Recursively free the pair */
                freeObj(obj->data.pair->car);
                freeObj(obj->data.pair->cdr);
                free(obj->data.pair);
        }
        free(obj);
}

/* 
 * Debugging
 *
 */

void printObj(Object *obj) {
        if (obj == NULL) {
                fprintf(stderr, "error: missing object");
        } else if (obj->type == NUM) {
                /* XXX: remove trailing zeroes */
                fprintf(stderr, "%f", obj->data.number);
        } else if (obj->type == SYM) {
                fprintf(stderr, "%s", obj->data.symbol);
        } else if (obj->type == PAIR) {
                fprintf(stderr, "(");
                printObj(car(obj));
                fprintf(stderr, " . ");
                printObj(cdr(obj));
                fprintf(stderr, ")");
        } else if (obj->type == NIL) {
                fprintf(stderr, "'()");
        } else {
                fprintf(stderr, "error: unknown object type\n");
        }
}

void printObj_newline(Object *obj) {
        printObj(obj);
        fprintf(stderr, "\n");
}

/* 
 * S-Expression parser 
 *
 */

/* Parse the tokens returned by getTokens() */
Object *parseTokens(Stack **s) {
        Object *obj;
        Object *expr = NULL;
        char *tok;
        float num;

        tok = stack_pop(s);
        if (tok == NULL)
                return NULL;

        if (strcmp(tok, ")") == 0) {
                free(tok);
                /* begin list processing with
                 * an empty list */
                expr = newNil();
                if (expr == NULL)
                        return NULL;
        } else {
                /* just return an atom */
                if (isNum(tok)) {
                        num = atof(tok);
                        obj = newNum(&num);
                        free(tok);
                        return obj;
                } else {
                        obj = newSym(tok);
                        free(tok);
                        return obj;
                }
        }
        /* fetch tokens from stack */
        while (!stack_isempty(*s)) {
                tok = stack_pop(s);
                /* examine token */
                if (strcmp(tok, ")") == 0) {
                        /* push it back on the stack
                         * and perform recursion */
                        stack_push(s, tok);
                        obj = parseTokens(s);
                } else if (strcmp(tok, "(") == 0) {
                        free(tok);
                        break;
                } else if (isNum(tok)) {
                        num = atof(tok);
                        obj = newNum(&num);
                        if (obj == NULL)
                                break;
                        free(tok);
                } else {
                        obj = newSym(tok);
                        free(tok);
                }
                expr = cons(obj, expr);
        }

        return expr;
}

/* Push all the tokens in to a stack and return it 
 * XXX: write this a bit more elegantly */
Stack *getTokens(char *buf) {
        Stack *s = stack_new();
        char atom[MAXLEN];
        char *str;
        int i;
        int depth = 0;
        /* track the depth of quotes */
        Stack *qstack = stack_new();
        int *qdepth;

        while (*buf != '\0') {
                if (*buf ==  '(') {
                        depth++;
                        buf++;
                        str = strdup("(");
                        stack_push(&s, str);
                        if (stack_top(s) != str) {
                                if (str != NULL)
                                        free(str);
                                break;
                        }
                } else if (*buf == ')') {
                        depth--;
                        buf++;
                        str = strdup(")");
                        stack_push(&s, str);
                        if (stack_top(s) != str) {
                                if (str != NULL)
                                        free(str);
                                break;
                        }
                } else if (*buf == '\'') {
                        buf++;
                        str = strdup("(");
                        stack_push(&s, str);
                        if (stack_top(s) != str) {
                                if (str != NULL)
                                        free(str);
                                break;
                        }
                        str = strdup("quote");
                        stack_push(&s, str);
                        if (stack_top(s) != str)
                                break;
                        /* push current depth on quote stack */
                        qdepth = malloc(sizeof(int));
                        if (qdepth == NULL)
                                break;
                        *qdepth = depth;
                        stack_push(&qstack, qdepth);
                        if (stack_top(qstack) != qdepth)
                                break;
                        /* skip quote stack check */
                        continue;
                } else if (*buf != ' ' && *buf != '\n') {
                        for (i = 0; i < MAXLEN-1; i++) {
                                /* stop at whitespace */
                                if (*buf == '\0' || *buf == ' ' || *buf == '\n')
                                        break;
                                /* stop at parens */
                                if (*buf == '(' || *buf == ')')
                                        break;
                                atom[i] = *buf++;
                        }
                        atom[i] = '\0';
                        str = strdup(atom);
                        stack_push(&s, str);
                        if (stack_top(s) != str) {
                                if (str != NULL)
                                        free(str);
                                break;
                        }
                } else {
                        /* eat whitespace */
                        while (*buf == ' ' || *buf == '\n')
                                buf++;
                }
                if (!stack_isempty(qstack)) {
                        if (depth == *(int *)stack_top(qstack)) {
                                free(stack_pop(&qstack));
                                str = strdup(")");
                                stack_push(&s, str);
                                if (stack_top(s) != str) {
                                        if (str != NULL)
                                                free(str);
                                        break;
                                }
                        }
                }
        }
        /* clean up */
        if (depth != 0 || !stack_isempty(qstack) || *buf != '\0') {
                stack_free(s);
                stack_free(qstack);
                return NULL;
        }
        return s;
}

int isDigit(char c) {
        return (c >= '0' && c <= '9');
}

/* Return non-zero if the string is real number */
int isNum(char *atom) {
        int decimalcount = 0;

        if (atom == NULL || *atom == '\0')
                return 0;
        /* check sign */
        if (*atom == '-') {
                if (*(atom + 1) == '\0')
                        return 0;
                atom++;
        }
        for (; *atom != '\0'; atom++) {
                if (*atom == '.') {
                        if (++decimalcount > 1)
                                break;
                } else if (!isDigit(*atom)) {
                        break;
                }
        }
        /* check if we reached the end */
        if (*atom == '\0') {
                /* remove decimal point if 
                   its a whole number */
                if (*atom == '.')
                        *atom = '\0';
                return 1;
        } 

        return 0;
}
