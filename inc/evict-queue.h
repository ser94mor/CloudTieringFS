#ifndef EVICT_QUEUE_H
#define EVICT_QUEUE_H

#include <sys/types.h>

struct __evict_node {
    const char *file_name;
    struct __evict_node *next;
};

struct __evict_queue {
    struct __evict_node *head;
    struct __evict_node *tail;
    size_t size;
    size_t max_size;
};

typedef struct __evict_node ev_n;
typedef struct __evict_queue ev_q;

int ev_q_empty(ev_q *queue);

void ev_q_push(ev_q *queue, ev_n *node);
void ev_q_pop(ev_q *queue);

ev_n *ev_q_front(ev_q *queue);
ev_n *ev_q_back(ev_q *queue);

ev_q *ev_q_alloc(size_t max_size);
ev_n *ev_n_alloc(const char *dir_name, const char *file_name);

void ev_q_free(ev_q *queue);
void ev_n_free(ev_n *node);

const char *ev_q_str(ev_q *queue);

#endif // EVICT_QUEUE_H
