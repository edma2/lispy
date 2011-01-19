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

        /* (loop (print (eval (read)))) */
        while (1) {
                print(obj = read(stdin));
                /* cleanup */
                freeObj(obj);
        }

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
                expr = CONS(obj, expr);
        }

        return expr;
}

/* Push all the tokens in to a stack and return it 
 * XXX: write this a bit more elegantly */
Stack *getTokens(char *buf) {
        Stack *s = stack_new();
        char atom[ATOMLEN];
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
