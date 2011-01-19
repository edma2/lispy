/* lisp.c - minimal lisp interpreter 
 * author: Eugene Ma (edma2) */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "stack.h"

#define ATOMLEN         100
#define EMPTY_LIST      NULL

/* Typing */
enum { TYPE_ATOM_SYMBOL, TYPE_ATOM_NUMBER, TYPE_PAIR, TYPE_EMPTY_LIST };
enum { STATE_BEGIN, STATE_OPEN, STATE_CLOSE, STATE_ATOM, STATE_ERROR };

typedef struct Pair Pair;
typedef union {
        char *symbol;
        float number;   /* Store numbers as floats for speed benefit */
        Pair *pair;
} Data;

typedef struct {
        Data *data;
        int type;       /* See type enumeration */
} Object;

struct Pair {
        Object *car;
        Object *cdr;
};

Object *car(Object *obj);
Object *cdr(Object *obj);
void car_set(Object *obj, Object *car);
void cdr_set(Object *obj, Object *cdr);
Object *obj_new(void *data, int type);
Object *number_new(float *f);
Object *symbol_new(char *symbol);
Object *emptylist_new(void);
Object *cons(Object *car, Object *cdr);
Object *parse(char *buf);
Stack *gettok(char *buf);
Object *parsetok(Stack **s);
void obj_free(Object *obj);
void obj_print(Object *obj);
void obj_print_nice(Object *obj);
int is_num(char *atom);
int is_digit(char c);

/*
 * Mutators
 *
 */

void car_set(Object *obj, Object *car) {
        if (obj == NULL)
                return;
        obj->data->pair->car = car;
}

void cdr_set(Object *obj, Object *cdr) {
        if (obj == NULL)
                return;
        obj->data->pair->cdr = cdr;
}

/*
 * Accessors
 *
 */

Object *car(Object *obj) {
        if (obj == NULL)
                return NULL;
        if (obj->type != TYPE_PAIR)
                return NULL;
        return obj->data->pair->car;
}

Object *cdr(Object *obj) {
        if (obj == NULL)
                return NULL;
        if (obj->type != TYPE_PAIR)
                return NULL;
        return obj->data->pair->cdr;
}

/*
 * Constructors
 *
 */

/* Instantiate a new Lisp object, which can be either a PAIR or an ATOM */
Object *obj_new(void *data, int type) {
        Object *obj;
        Data *d;

        obj = malloc(sizeof(Object));
        if (obj == NULL) {
                return NULL;
        }

        /* An empty list is represented as a NULL pointer to object's data */
        if (data == NULL) {
                obj->data = EMPTY_LIST;
                obj->type = TYPE_EMPTY_LIST;
                /* Any other kind of data will be typed and if necessary, allocated memory */
        } else {
                d = malloc(sizeof(Data));
                if (d == NULL) {
                        free(obj);
                        return NULL;
                }
                if (type == TYPE_ATOM_SYMBOL) {
                        /* Free symbol string when we are done with the object */
                        d->symbol = strdup((char *)data);
                        /* Check that strcup succeeded */
                        if (d->symbol == NULL) {
                                free(d);
                                free(obj);
                                return NULL;
                        }
                } else if (type == TYPE_ATOM_NUMBER) {
                        d->number = *(float *)data; 
                } else if (type == TYPE_PAIR) {
                        d->pair = (Pair *)data;
                }
                obj->data = d;
                obj->type = type;
        }

        return obj;
}

Object *symbol_new(char *symbol) {
        return obj_new(symbol, TYPE_ATOM_SYMBOL);
}

Object *number_new(float *f) {
        return obj_new(f, TYPE_ATOM_NUMBER);
}

Object *emptylist_new(void) {
        return obj_new(EMPTY_LIST, TYPE_EMPTY_LIST);
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
        obj = obj_new(p, TYPE_PAIR);
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

void obj_free(Object *obj) {
        if (obj == NULL)
                return;
        if (obj->type == TYPE_ATOM_SYMBOL) {
                free(obj->data->symbol);
        } else if (obj->type == TYPE_PAIR) {
                /* Recursively free the pair */
                obj_free(obj->data->pair->car);
                obj_free(obj->data->pair->cdr);
                free(obj->data->pair);
        }
        free(obj->data);
        free(obj);
}

/* 
 * Debugging
 *
 */

void obj_print(Object *obj) {
        if (obj == NULL) {
                fprintf(stderr, "error: missing object");
        } else if (obj->type == TYPE_ATOM_NUMBER) {
                /* XXX: remove trailing zeroes */
                fprintf(stderr, "%f", obj->data->number);
        } else if (obj->type == TYPE_ATOM_SYMBOL) {
                fprintf(stderr, "%s", obj->data->symbol);
        } else if (obj->type == TYPE_PAIR) {
                fprintf(stderr, "(");
                obj_print(car(obj));
                fprintf(stderr, " . ");
                obj_print(cdr(obj));
                fprintf(stderr, ")");
        } else if (obj->type == TYPE_EMPTY_LIST) {
                fprintf(stderr, "'()");
        } else {
                fprintf(stderr, "error: unknown object type\n");
        }
}

void obj_print_nice(Object *obj) {
        obj_print(obj);
        fprintf(stderr, "\n");
}

/* 
 * S-Expression parser 
 *
 */

Object *parse(char *expr) {
        /* XXX */
        return NULL;
}

/* Parse the tokens returned by gettok() */
Object *parsetok(Stack **s) {
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
                expr = emptylist_new();
                if (expr == NULL)
                        return NULL;
        } else {
                /* just return an atom */
                if (is_num(tok)) {
                        num = atof(tok);
                        obj = number_new(&num);
                        free(tok);
                        return obj;
                } else {
                        obj = symbol_new(tok);
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
                        obj = parsetok(s);
                } else if (strcmp(tok, "(") == 0) {
                        free(tok);
                        break;
                } else if (is_num(tok)) {
                        num = atof(tok);
                        obj = number_new(&num);
                        if (obj == NULL)
                                break;
                        free(tok);
                } else {
                        obj = symbol_new(tok);
                        free(tok);
                }
                expr = cons(obj, expr);
        }

        return expr;
}

/* Push all the tokens in to a stack and return it */
Stack *gettok(char *buf) {
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

int is_digit(char c) {
        return (c >= '0' && c <= '9');
}

/* Return non-zero if the string is real number */
int is_num(char *atom) {
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
                } else if (!is_digit(*atom)) {
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

int main(void) {
        Stack *s;
        Object *obj;

        s = gettok("'(1 2 3)");
        obj = parsetok(&s);
        obj_print_nice(obj);
        obj_free(obj);

        return 0;
}
