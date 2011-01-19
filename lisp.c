/* lisp.c - minimal lisp interpreter 
 * author: Eugene Ma (edma2)
 * TODO: environments, lambda procs
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "stack.h"

#define ATOMLEN         100
#define READLEN         1000
#define ISDIGIT(c)      (c >= '0' && c <= '9')
#define CAR(X)          (X->data.pair->car)
#define CDR(X)          (X->data.pair->cdr)
#define SETCAR(X, A)    (CAR(X) = A)
#define SETCDR(X, A)    (CDR(X) = A)
#define CONS(X, Y)      (newPair(X, Y))

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

Object *newObj(void *data, enum otype type);
Object *newNum(float *f);
Object *newSym(char *symbol);
Object *newPair(Object *car, Object *cdr);
Object *newNil(void);
Stack *getTokens(char *buf);
Object *parseTokens(Stack **s);
void freeObj(Object *obj);
void printObj(Object *obj, int cancelout);
int isNum(char *atom);

Object *read(FILE *f);
Object *eval(Object *obj, Object *env); //todo
void print(Object *obj);

int main(void) {
        Object *obj;

        print(obj = read(stdin));

        /* (loop (print (eval (read))))
        while (1) {
                print(obj = read(stdin));
                freeObj(obj);
        }
        */

        return 0;
}

Object *read(FILE *f) {
        char buf[READLEN];
        Stack *s;

        if (fgets(buf, READLEN, f) == NULL)
                return NULL;
        /* remove newline */
        buf[strlen(buf) - 1] = '\0';
        s = getTokens(buf);

        return parseTokens(&s);
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

Object *newPair(Object *car, Object *cdr) {
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

void printObj(Object *obj, int flag_cancel) {
        if (obj == NULL) {
                fprintf(stderr, "error: missing object");
        } else if (obj->type == NUM) {
                /* XXX: remove trailing zeroes */
                fprintf(stderr, "%f", obj->data.number);
        } else if (obj->type == SYM) {
                fprintf(stderr, "%s", obj->data.symbol);
        } else if (obj->type == PAIR) {
                if (flag_cancel == 0)
                        fprintf(stderr, "(");
                printObj(CAR(obj), 0);
                /* check next object */
                if (CDR(obj)->type == SYM || CDR(obj)->type == NUM) {
                        fprintf(stderr, " . ");
                        printObj(CDR(obj), 0);
                        fprintf(stderr, ")");
                } else {
                        if (CDR(obj)->type != NIL)
                                fprintf(stderr, " ");
                        printObj(CDR(obj), 1);
                }
        } else if (obj->type == NIL) {
                if (flag_cancel == 0)
                        fprintf(stderr, "(");
                fprintf(stderr, ")");
        } else {
                fprintf(stderr, "error: unknown object type\n");
        }
}

void print(Object *obj) {
        printObj(obj, 0);
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

        /* fetch tokens from stack */
        while (!stack_isempty(*s)) {
                tok = stack_pop(s);
                /* examine token */
                if (strcmp(tok, ")") == 0) {
                        if (expr == NULL) {
                                free(tok);
                                expr = newNil();
                                continue;
                        } else {
                                /* put it back on the stack and recurse */
                                stack_push(s, tok);
                                obj = parseTokens(s);
                                /* check the top for quote */
                                while (!stack_isempty(*s) && strcmp((char *)stack_top(*s), "\'") == 0) {
                                        /* pop it off */
                                        free(stack_pop(s));
                                        obj = CONS(newSym("quote"), CONS(obj, newNil()));
                                }
                        }
                } else if (strcmp(tok, "(") == 0) {
                        free(tok);
                        break;
                } else if (isNum(tok)) {
                        num = atof(tok);
                        free(tok);
                        obj = newNum(&num);
                        if (obj == NULL)
                                break;
                        /* number atom */
                        if (expr == NULL) {
                                expr = obj;
                                break;
                        }
                } else if (strcmp(tok, "\'") != 0) {
                        obj = newSym(tok);
                        free(tok);
                        if (obj == NULL)
                                break;
                        /* symbol atom */
                        if (expr == NULL) {
                                expr = obj;
                                break;
                        }
                }
                expr = CONS(obj, expr);
        }
        while (!stack_isempty(*s) && strcmp((char *)stack_top(*s), "\'") == 0) {
                /* pop it off */
                free(stack_pop(s));
                expr = CONS(newSym("quote"), CONS(expr, newNil()));
        }

        return expr;
}

/* Push all the tokens in to a stack and return it */
Stack *getTokens(char *buf) {
        Stack *s = stack_new();
        char atom[ATOMLEN];
        char *str;
        int i;

        while (*buf != '\0') {
                if (*buf ==  '(') {
                        str = strdup("(");
                        stack_push(&s, str);
                        buf++;
                } else if (*buf == ')') {
                        str = strdup(")");
                        stack_push(&s, str);
                        buf++;
                } else if (*buf == '\'') {
                        str = strdup("\'");
                        stack_push(&s, str);
                        buf++;
                } else if (*buf != ' ' && *buf != '\n') {
                        for (i = 0; i < ATOMLEN-1; i++) {
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
                } else {
                        /* eat whitespace */
                        while (*buf == ' ' || *buf == '\n')
                                buf++;
                }
        }
        /* clean up */
        if (*buf != '\0') {
                stack_free(s);
                return NULL;
        }
        return s;
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
                } else if (!ISDIGIT(*atom)) {
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
