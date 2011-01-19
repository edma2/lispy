#include "stack.h"

/* 
 * [item]-->[item]-->[item]-->[item]-->NULL
 * ^ top                       ^bottom
 */

Stack *stack_new(void) {
        return NULL;
}

int stack_isempty(Stack **s) {
        return (*s == NULL);
}

void *stack_top(Stack **s) {
        if (stack_isempty(s))
                return NULL;
        return (*s)->item;
}

void stack_free(Stack **s) {
        while (!stack_isempty(s))
                stack_pop(s);
}

/* Push new item onto stack */
void stack_push(Stack **s, void *item) {
        Stack *top;

        top = malloc(sizeof(Stack));
        if (top == NULL)
                return;
        top->item = item;
        top->next = *s;
        *s = top;
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
