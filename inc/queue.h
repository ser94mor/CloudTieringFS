#ifndef QUEUE_H
#define QUEUE_H

#include <stdio.h>
#include <sys/types.h>

typedef struct {
    char *head;
    char *tail;
    size_t cur_q_size;
    size_t max_q_size;
    size_t max_item_size;
    char *buffer;
    size_t buffer_size;
} queue_t;

int queue_empty(queue_t *q);
int queue_full(queue_t *q);

int queue_push(queue_t *q, char *item, size_t item_size);
int queue_pop(queue_t *q);

char *queue_front(queue_t *q, size_t *size);

queue_t *queue_alloc(size_t max_q_size, size_t max_item_size);
void queue_free(queue_t *q);

void queue_print(FILE *stream, queue_t *q);

#endif // QUEUE_H
