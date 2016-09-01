#ifndef EVICT_QUEUE_H
#define EVICT_QUEUE_H

#include <sys/types.h>
#include <linux/fs.h>
#include <linux/limits.h>

struct __evict_node {
    const char *dir_name;
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

// NEW CODE BELOW

struct __file_path {
    const char name[NAME_MAX];
    const size_t len;
    const struct file_path *ancestor;
    const size_t depth;
    const int is_leaf;

};

/*
 * Directory depth value cannot be more then PATH_MAX / 2 because
 * path that has a maximum depth is /a/a/a/.../a (consists 1-letter name for each depth)
 */
const char *depth_file_path_map[PATH_MAX / 2];

#endif // EVICT_QUEUE_H
