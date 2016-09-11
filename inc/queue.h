#ifndef QUEUE_H
#define QUEUE_H

#include <sys/types.h>

typedef struct {
    ev_n *head;
    ev_n *tail;
    size_t cur_q_size;
    size_t max_q_size;
    size_t max_item_size;
    void *buffer;
    size_t buffer_size;
    void *splitted_item;
} queue_t;

int queue_empty(queue_t *q);
int queue_full(queue_t *q);

int queue_push(queue_t *q, void *item, size_t item_size);
int queue_pop(queue_t *q);

void *queue_front(queue_t *q, size_t *size);
/* void *queue_back(queue_t *q, size_t *size); */

queue_t *queue_alloc(size_t max_q_size, size_t max_item_size);
void queue_free(queue_t *q);

const char *queue_str(queue_t *q);

#endif // QUEUE_H
