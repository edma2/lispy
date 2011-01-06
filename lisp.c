/* lisp.c - minimal lisp interpreter 
 * author: Eugene Ma (edma2) */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
Object *obj_new(void *data, int type);
Object *number_new(float *f);
Object *symbol_new(char *symbol);
Object *cons(Object *car, Object *cdr);
Object *parse(char *buf);
void obj_free(Object *obj);
void obj_print(Object *obj);

  /*
   * Mutators
   *
   */

void set_car(Object *obj, Object *car) {
        if (obj == NULL)
                return;
        obj->data->pair->car = car;
}

void set_cdr(Object *obj, Object *cdr) {
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
        return obj_new(f, TYPE_ATOM_SYMBOL);
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
                fprintf(stderr, "error: missing object\n");
        } else if (obj->type == TYPE_ATOM_NUMBER) {
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

/* S-Expression parser - modeled as a finite state machine, returns a PAIR or an ATOM */
Object *parse(char *buf) {
        char word[ATOMLEN];
        int state;
        int i = 0;
        int layer = 0;
        int quoted = -1;

        if (buf == NULL)
                return NULL;

        state = STATE_BEGIN;
        do {
                switch (state) {
                        /* Initial state */
                        case STATE_BEGIN:
                                if (*buf == '\0') {
                                        fprintf(stderr, "Missing expression\n");
                                        state = STATE_ERROR;
                                } else if (*buf == '(') {
                                        fprintf(stderr, "Going down a layer\n");
                                        layer++;
                                        state = STATE_OPEN;
                                } else if (*buf == ')') {
                                        fprintf(stderr, "syntax error: premature closing paren\n");
                                        state = STATE_ERROR;
                                } else if (*buf == '\'') {
                                        /* Append a quotation in front of every ATOM in
                                         * the list until we reach a closing paren */
                                        quoted = layer;
                                /* If we encounter a non-space character, start feeding it
                                 * into the word buffer */
                                } else if (*buf != ' ' && *buf != '\n') {
                                        i = 0;
                                        if (quoted >= 0)
                                                word[i++] = '\'';
                                        word[i++] = *buf;
                                        state = STATE_ATOM;
                                }
                                break;
                        case STATE_OPEN:
                                if (*buf == '(') {
                                        fprintf(stderr, "Going down a layer\n");
                                        layer++;
                                } else if (*buf == ')') {
                                        /* Only valid if input is a quoted empty list */
                                        if (quoted < 0) {
                                                fprintf(stderr, "Premature closing paren\n");
                                                state = STATE_ERROR;
                                                break;
                                        }
                                        layer--;
                                        /* Check if quoting finished */
                                        if (layer == quoted)
                                                quoted = -1;
                                        fprintf(stderr, "Going up a layer\n");
                                        state = STATE_CLOSE;
                                } else if (*buf == '\'') {
                                        /* Ignore multiple quotations */
                                        if (quoted < 0)
                                                quoted = layer;
                                } else if (*buf != ' ' && *buf != '\n') {
                                        i = 0;
                                        /* Append a quote in front of the word */
                                        if (quoted >= 0)
                                                word[i++] = '\'';
                                        word[i++] = *buf;
                                        state = STATE_ATOM;
                                }
                                break;
                         case STATE_CLOSE:
                                if (*buf == '(') {
                                        fprintf(stderr, "Going down a layer\n");
                                        layer++;
                                        state = STATE_OPEN;
                                } else if (*buf == ')') {
                                        fprintf(stderr, "Going up a layer\n");
                                        layer--;
                                        if (layer == quoted)
                                                quoted = -1;
                                } else if (*buf == '\'') {
                                        if (quoted < 0)
                                                quoted = layer;
                                } else if (*buf != ' ' && *buf != '\n') {
                                        i = 0;
                                        /* Append a quote in front of the word */
                                        if (quoted >= 0)
                                                word[i++] = '\'';
                                        word[i++] = *buf;
                                        state = STATE_ATOM;
                                }
                                break;
                         /* Being in this state means we are currently feeding ATOMs
                          * into the list. We transition into this state after we see 
                          * an open paren a followed by a non-space character. We 
                          * leave this state when we reach the closeing paren. */
                         case STATE_ATOM:
                                if (*buf == '(') {
                                        if (i != 0) {
                                                word[i] = '\0';
                                                i = 0;
                                                fprintf(stderr, "Adding word %s\n", word);
                                        }
                                        fprintf(stderr, "Going down a layer\n");
                                        layer++;
                                        state = STATE_OPEN;
                                } else if (*buf == ')') {
                                        if (i != 0) {
                                                word[i] = '\0';
                                                i = 0;
                                                fprintf(stderr, "Adding word %s\n", word);
                                        }
                                        fprintf(stderr, "Going up a layer\n");
                                        /* Finished quoting everything between the parens */
                                        layer--;
                                        if (layer == quoted);
                                                quoted = -1;
                                        state = STATE_CLOSE;
                                } else if (*buf == '\'') {
                                        quoted = 1;
                                } else if (*buf != ' ' && *buf != '\n') {
                                        if (i == 0 && (quoted >= 0))
                                                word[i++] = '\'';
                                        word[i++] = *buf;
                                } else {
                                        /* Skip space chars and do not do anything unless
                                         * buffer is non-empty. If buffer is non-empty then
                                         * i != 0, flush the buffer. Remain in this state */
                                        if (i != 0) {
                                                word[i] = '\0';
                                                fprintf(stderr, "Adding word %s\n", word);
                                                i = 0;
                                        }
                                }
                                break;
                         default:
                                fprintf(stderr, "Unknown state: %d\n", state);
                                state = STATE_ERROR;
                }
                buf++;
        } while (state != STATE_ERROR && *buf != '\0');

        if (layer != 0)
                fprintf(stderr, "lisp: mismatched parens\n");
        /* Take care of ATOMS */
        if (state == STATE_ATOM && i != 0) {
                word[i] = '\0';
                i = 0;
                fprintf(stderr, "Adding word %s\n", word);
        }

        return NULL;
}

int main(void) {
        parse("'()");

        return 0;
}
