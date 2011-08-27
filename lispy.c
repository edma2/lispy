/* 
 * Minimal LISP interpreter - minlisp.c
 * Written by Eugene Ma (edma2)
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "stack.h"

#define MAXCHAR         100
#define MAXLINE         1000
#define car(x)          ((x->data.pair)[0])
#define cdr(x)          ((x->data.pair)[1])
#define setcar(x,a)     (car(x) = (a))
#define setcdr(x,a)     (cdr(x) = (a))
#define cons(x, y)      (new_pair(x, y))
#define is_digit(c)     (c >= '0' && c <= '9')
#define whitespace(c)   (c == ' ' || c == '\n')
#define reserved(c)     (c == '(' || c == ')' || c == '\'')
#define is_emptylist(x) (x->type == NIL)
#define cmp_sym(a, b)   (strcmp(a->data.symbol, b->data.symbol))

enum otype { SYM, NUM, PAIR, NIL };
struct Object {
        union {
                char *symbol;
                float number;
                struct Object *pair[2];
        } data;
        enum otype type;
};
typedef struct Object Object;

Object *mkobj(void *data, enum otype type);
Object *new_number(float f);
Object *new_symbol(char *symbol);
Object *new_pair(Object *car, Object *cdr);
Object *new_nil(void);
Object *read(FILE *fp);
Object *parse(Stack **s);
char *read_token(FILE *fp);
Stack *tokenize(FILE *fp);
void print(FILE *fp, Object *obj);
void printobj(FILE *fp, Object *obj);
void freeobj(Object *obj);

int isnum(char *atom);
int is_list(Object *obj);

int main(void) {
#if 0
        /* (loop (print (eval (read) global))) */
        while (1) {
                printf("> ");
                readobj = read(stdin);
                print(stdout, readobj);
        }
#endif
        Stack *s = tokenize(stdin);
        Object *list = parse(&s);
        print(stdout, list);
        freeobj(list);

        return 0;
}

/* 
 * Returns non-zero if the object if a list 
 */
int is_list(Object *obj) {
        for (; obj->type == PAIR; obj = cdr(obj))
                ;
        return obj->type == NIL;
}

/* 
 * Returns non-zero if the string is real number.
 */
int isnum(char *atom) {
        for (; is_digit(*atom); atom++)
                ;
        if (*atom == '.') {
                for (atom++; is_digit(*atom); atom++)
                        ;
        }
        return (*atom == '\0');
}

/* 
 * Reads user input and returns the expression as a Lisp list.
 */
Object *read(FILE *fp) {
        Stack *s;
        s = tokenize(fp);
        return parse(&s);
}

/*
 * Print the object in a readable and sane format.
 */
void print(FILE *fp, Object *obj) {
        printobj(fp, obj);
        fprintf(fp, "\n");
}

/*
 * Allocate memory on the heap for a new Object. 
 * Data argument is a pointer to a string, a float, a two element array of
 * pointers, or NULL.
 *
 * Returns pointer to new Object, or NULL on error.
 */
Object *mkobj(void *data, enum otype type) {
        Object *obj;

        obj = malloc(sizeof(Object));
        if (obj == NULL)
                return NULL;

        obj->type = type;
        switch (type) {
                case SYM:
                        /* Duplicate a copy of the string on the heap */
                        obj->data.symbol = strdup((char *)data);
                        if (obj->data.symbol == NULL) {
                                free(obj);
                                return NULL;
                        }
                        break;
                case NUM:
                        obj->data.number = *(float *)data; 
                        break;
                case PAIR:
                        obj->data.pair[0] = ((Object **)data)[0];
                        obj->data.pair[1] = ((Object **)data)[1];
                        break;
                case NIL:
                        break;
        }

        return obj;
}

/* 
 * Create a new symbol Object
 */
inline Object *new_symbol(char *symbol) {
        return mkobj(symbol, SYM);
}

/* 
 * Create a new number Object
 */
inline Object *new_number(float f) {
        return mkobj(&f, NUM);
}

/* 
 * Create a new nil Object (also empty list)
 */
inline Object *new_nil(void) {
        return mkobj(NULL, NIL);
}

/* 
 * Create a new pair Object from two existing pairs.  
 * This is copy by reference, so the new pair will point to existing copies.
 */
Object *new_pair(Object *car, Object *cdr) {
        Object *p[2];

        if (car == NULL || cdr == NULL)
                return NULL;
        p[0] = car;
        p[1] = cdr;

        return mkobj(p, PAIR);
}

/*
 * Free the object. If object is a pair, then recursively free the car and cdr
 * elements and so on.
 * This needs work.
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
 * Print a nicely formatted object.
 */
void printobj(FILE *fp, Object *obj) {
        switch (obj->type) {
                case NUM:
                        fprintf(fp, "%f", obj->data.number);
                        break;
                case SYM:
                        fprintf(fp, "%s", obj->data.symbol);
                        break;
                case PAIR:
                        fprintf(fp, "(");
                        printobj(fp, car(obj));
                        fprintf(fp, " . ");
                        printobj(fp, cdr(obj));
                        fprintf(fp, ")");
                        break;
                default:
                        /* nil */
                        fprintf(fp, "()");
                        break;
        }
}

/* 
 * Converts a stack of tokens (generated using tokenize()) into a LISP list we
 * can traverse using internal car and cdr functions. We can assume the stack
 * is syntactically valid with matching parens.
 *
 * Returns a pointer to an Object of type PAIR.
 */
Object *parse(Stack **s) {
        Object *list = NULL;
        Object *obj;
        char *tok;

        /* Build list backwards; last close paren is on top of stack. */
        while (!stack_empty(s)) {
                tok = stack_pop(s);
                if (tok == NULL)
                        break;

                if (tok[0] == ')') {
                        if (list == NULL) {
                                /* Start new list */
                                list = new_nil();
                                free(tok);
                        } else {
                                /* Put it back */
                                if (stack_push(s, tok) == NULL)
                                        break;
                                /* Parse inner list recursively */
                                list = cons(parse(s), list);
                        }
                } else if (tok[0] == '(') {
                        free(tok);
                        break;
                } else {
                        obj = isnum(tok) ? new_number(atof(tok)) : new_symbol(tok);
                        /* Eat inner quotes: ('foo) */
                        while (!stack_empty(s) && ((char *)stack_top(s))[0] == '\'') {
                                free(stack_pop(s));
                                obj = cons(new_symbol("quote"), cons(obj, new_nil()));
                        }
                        list = (list) ? cons(obj, list) : obj;
                        free(tok);
                }
                /* malloc() failed */
                if (list == NULL)
                        break;
        }
        /* Outer quotes: '(1 2 3) */
        while (!stack_empty(s) && ((char *)stack_top(s))[0] == '\'') {
                free(stack_pop(s));
                list = cons(new_symbol("quote"), cons(list, new_nil()));
        }
        return list;
}

/* 
 * Read the next token from an input stream.
 *
 * A token is either a special character, such as a paren or a single quote, or
 * is a whitespace/symbol delimited "word".
 * Example: (+ 1 2 3) => tokenized => '(', '+', '1', '2', '3', ')'
 *
 * Returns NULL on EOF, or pointer to a heap-copy of the token.
 */ 
char *read_token(FILE *input) {
        char tok[MAXCHAR];
        char c;
        int i;

        /* Eat up whitespace */
        while ((c = getc(input))) {
                if (c == EOF)
                        return NULL;
                if (!whitespace(c))
                        break;
        } 

        /* If we encounter a reserved character, prepare token. */
        if (reserved(c)) {
                tok[0] = c;
                tok[1] = '\0';
        } else {
                /* 
                 * Otherwise, process the entire string until a whitespace or
                 * reserved symbol is encountered.
                 */
                for (i = 0; i < MAXCHAR; i++) {
                        if (whitespace(c) || reserved(c)) {
                                /* Feed character back into input stream */
                                ungetc(c, input);
                                tok[i] = '\0';
                                break;
                        }
                        tok[i] = c;
                        /* Get next character */
                        c = getc(input);
                        if (c == EOF)
                                return NULL;
                }
        }
        return strdup(tok);
}

/* 
 * Read a line and tokenize it.
 * A valid expression is either a list (possibly deep) with matching parens, or
 * an atom. See get_token() for the formal definition of a token.
 *
 * Returns a pointer to a Stack.
 */
Stack *tokenize(FILE *input) {
        char *tok;
        int depth = 0;
        Stack *s = NULL;

        do {
                tok = read_token(stdin);
                /* EOF */
                if (tok == NULL)
                        break;
                if (tok[0] == '(')
                        depth++;
                if (stack_push(&s, tok) == NULL)
                        /* malloc() failed */
                        break;
                else if (tok[0] == ')')
                        depth--;
        } while (depth != 0 || tok[0] == '\'');

        /* Early EOF or malloc() error */
        while (depth && s)
                free(stack_pop(&s));

        return s;
}
