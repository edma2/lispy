#include <stdlib.h>

struct Stack {
        struct Stack *next;
        void *item;
};
typedef struct Stack Stack;

int stack_empty(Stack **s);
void *stack_top(Stack **s);
void *stack_push(Stack **s, void *item);
void *stack_pop(Stack **s);
