#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "evict-queue.h"

#define NULL_STR_PATTERN "        {\n"\
                         "        },\n"

/* length of NULL_STR_PATTERN */
#define NULL_STR_PATTERN_LEN    10 + 11

#define EV_N_STR_PATTERN "        {\n"\
                         "            \"dir_name\": \"%s\",\n"\
                         "            \"file_path\": \"%s\"\n"\
                         "        },\n"

/* length of EV_N_STR_PATTERN with excluded "%s"s */
#define EV_N_STR_PATTERN_LEN    10 + 28 + 28 + 11

#define EV_Q_STR_PATTERN "{\n"\
                         "    \"size\": %zd,\n"\
                         "    \"max_size\": %zd,\n"\
                         "    \"nodes\": [\n"\
                         "%s\n"\
                         "    ]\n"\
                         "}\n"

/* length of EV_Q_STR_PATTERN with excluded "%s"s and 20 chars per "%zd" */
#define EV_Q_STR_PATTERN_LEN    2 + 34 + 38 + 15 + 1 + 6 + 2

/*
 * Evict queue implementation.
 */

/**
 *  Implementation of "empty" queue method.
 */
int ev_q_empty(ev_q *queue) {
    if (queue == NULL) {
        return -1;
    }

    return (queue->size == 0);
}

/**
 * Implementation of "push" queue method.
 */
void ev_q_push(ev_q *queue, ev_n *node) {
    if (queue == NULL || node == NULL) {
        return;
    }

    if (queue->size == 0) {
        queue->head = node;
    } else {
        queue->tail->next = node;
    }

    queue->tail = node;
    node->next = NULL;
    (queue->size)++;
}

/**
 * Implementation of "pop" queue method.
 */
void ev_q_pop(ev_q *queue) {
    if (queue == NULL || queue->size == 0) {
        return;
    }

    ev_n *node = queue->head;
    queue->head = queue->head->next;
    (queue->size)--;

    if (queue->size == 0) {
        queue->head = NULL;
        queue->tail = NULL;
    }

    ev_n_free(node);
}

/**
 * Implementation of "front" queue operation.
 */
ev_n *ev_q_front(ev_q *queue) {
    if (queue == NULL) {
        return NULL;
    }

    return queue->head;
}

/**
 * Implementation of "back" queue operation.
 */
ev_n *ev_q_back(ev_q *queue) {
    if (queue == NULL) {
        return NULL;
    }

    return queue->tail;
}

/**
 * Allocates memory for ev_q and initializes max_size with
 * the given value.
 */
ev_q *ev_q_alloc(size_t max_size) {
    ev_q *queue = malloc(sizeof(ev_q));
    queue->max_size = max_size;
    queue->size = 0;
    queue->head = NULL;
    queue->tail = NULL;

    return queue;
}

/**
 * Frees all memory allocated for the given ev_q pointer.
 */
void ev_q_free(ev_q *queue) {
    if (queue == NULL) {
        return;
    }

    while (queue->size > 0) {
        ev_n *node = queue->head;
        queue->head = queue->head->next;
        ev_n_free(node);
        (queue->size)--;
    }
    free(queue);
}

/**
 * Allocates memory for ev_q_node and initializes file_path with
 * the given value.
 */
ev_n *ev_n_alloc(const char *dir_name, const char *file_name) {
    if (dir_name == NULL || file_name == NULL) {
        return NULL;
    }

    ev_n *node = malloc(sizeof(ev_n));

    /* copy strings to a separate memory area in order to be independent
     * on the outer world (might be revised in the future;
     * will cause performance impact) */
    size_t fn_size = (strlen(file_name) + 1) * sizeof(char);
    size_t dn_size = (strlen(dir_name)  + 1) * sizeof(char);
    char *fn = strcpy(malloc(fn_size), file_name);
    char *dn = strcpy(malloc(dn_size), dir_name);

    node->file_name = (const char *)fn;
    node->dir_name  = (const char *)dn;
    node->next = NULL;

    return node;
}

/**
 * Frees all memory allocated for the given ev_q_node pointer.
 */
void ev_n_free(ev_n *node) {
    if (node == NULL) {
        return;
    }

    free((void *)node->file_name);
    free((void *)node->dir_name);
    free(node);
}

/**
 * String representation of ev_n. Function is not visible outside.
 */
const char *__ev_n_str(ev_n *node) {
    if (node == NULL) {
        /* normally this code will never be executed
         * because it is called only from ev_q_str */

        char *null_str = malloc((NULL_STR_PATTERN_LEN + 1) * sizeof(char));
        sprintf(null_str, NULL_STR_PATTERN);

        return (const char *)null_str;
    }

    size_t str_len = strlen(node->dir_name) +
                     strlen(node->file_name) +
                     EV_N_STR_PATTERN_LEN;
    char *str_buf = malloc((str_len + 1) * sizeof(char));
    sprintf(str_buf, EV_N_STR_PATTERN, node->dir_name, node->file_name);

    return (const char *)str_buf;
}

/**
 * String representation of ev_q. Function is visible outside.
 * The returned string should be freed.
 */
const char *ev_q_str(ev_q *queue) {
    if (queue == NULL) {
        char *null_str = malloc((NULL_STR_PATTERN_LEN + 1) * sizeof(char));

        sprintf(null_str, NULL_STR_PATTERN);

        return (const char *)null_str;
    }

    size_t total_node_str_len = 0;

    ev_n *cur_node = queue->head;
    while (cur_node != NULL) {
        total_node_str_len += strlen(cur_node->dir_name) +
                              strlen(cur_node->file_name) +
                              EV_N_STR_PATTERN_LEN;
        cur_node = cur_node->next;
    }

    char *total_node_str = calloc(total_node_str_len + 1, sizeof(char));

    cur_node = queue->head;
    while (cur_node != NULL) {
        const char *cur_node_str = __ev_n_str(cur_node);
        strcat(total_node_str, cur_node_str);
        free((void *)cur_node_str);
        cur_node = cur_node->next;
    }

    /* clean last ",\n" */
    total_node_str[total_node_str_len - 1] = 0;
    total_node_str[total_node_str_len - 2] = 0;

    size_t total_queue_str_len = total_node_str_len + EV_Q_STR_PATTERN_LEN;
    char *total_queue_str = calloc(total_queue_str_len + 1, sizeof(char));
    sprintf(total_queue_str, EV_Q_STR_PATTERN, queue->size, queue->max_size, total_node_str);

    free(total_node_str);
    return (const char *)total_queue_str;
}
