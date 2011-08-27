#include "stack.h"

/* 
 * [item]-->[item]-->[item]-->[item]-->NULL
 * ^ top                       ^bottom
 */

void *stack_top(Stack **s) {
        return (*s) ? (*s)->item : NULL;
}

int stack_empty(Stack **s) {
        return (*s) == NULL;
}

/* Push new item onto stack */
void *stack_push(Stack **s, void *item) {
        Stack *top;

        top = malloc(sizeof(Stack));
        if (top == NULL)
                return NULL;

        top->item = item;
        top->next = *s;
        *s = top;

        return top->item;
}

/* Pop stack and return item */
void *stack_pop(Stack **s) {
        Stack *next;
        void *item;

        if (*s == NULL)
                return NULL;
        next = (*s)->next;
        item = (*s)->item;
        free(*s);
        *s = next;

        return item;
}
