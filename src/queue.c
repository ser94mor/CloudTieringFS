#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"


/**
 * @brief queue_empty Indicates that queue is empty.
 * @return >0 if size equals 0; 0 if size greater then 0; <0 if queue equals NULL
 */
int queue_empty(queue_t *queue) {
        if (queue == NULL) {
                return -1;
        }
        return (queue->cur_q_size == 0);
}

/**
 * @brief queue_full Indicates that the queue is full.
 * @return >0 if size equals maximem queue size therwise 0; <0 if queue equals NULL
 */
int queue_full(queue_t *queue) {
        if (queue == NULL) {
                return -1;
        }
        return (queue->cur_q_size == queue->max_q_size);
}

/**
 * @brief queue_push Push item into queue if there is enough place for this item.
 * @return 0 on sussess; <0 on failure
 */
int queue_push(queue_t *queue, void *item, size_t item_size) {
        if (queue == NULL || item == NULL || item_size == 0 ||
            item_size > queue->max_item_size ||
            queue->cur_q_size == queue->max_q_size) {
                return -1;
        }

        // TODO: rewrite using modulo
        if ((queue->tail + item_size + sizeof(size_t)) <= (queue->buffer + queue->buffer_size)) {
                memcpy(queue->tail, &item_size, sizeof(size_t));
                queue->tail += sizeof(size_t);
                memcpy(queue->tail, item, item_size);
                queue->tail += item_size;
        } else {
                size_t bytes_left = (queue->buffer + queue->buffer_size) - queue->tail;
                size_t bytes_right = item_size + sizeof(size_t) - bytes_left;

                /* write size of item */
                if (sizeof(size_t) >= bytes_left) {
                        memcpy(queue->tail, &item_size, bytes_left);
                        queue->tail = queue->buffer;
                        memcpy(queue->tail, (void *)&item_size + bytes_left, sizeof(size_t) - bytes_left);
                        queue->tail += sizeof(size_t) - bytes_left;
                        memcpy(queue->tail, item, item_size);
                        queue->tail += item_size;
                } else {
                        memcpy(queue->tail, &item_size, sizeof(size_t));
                        queue->tail += sizeof(size_t);
                        memcpy(queue->tail, item, bytes_left - sizeof(size_t));
                        queue->tail = queue->buffer;
                        memcpy(queue->tail, item + (bytes_left - sizeof(size_t)), bytes_right);
                        queue->tail += bytes_right;
                }
        }

        return 0;
}

void queue_pop(ev_q *queue);

void *queue_front(queue_t *q, size_t *size);
void *queue_back(queue_t *q, size_t *size);

queue_t *queue_alloc(size_t max_q_size, size_t max_item_size);
void queue_free(queue_t *q);

const char *queue_str(queue_t *q);


/**
 * @brief ev_q_push Implementation of "push" queue method.
 * @return 0 on success; <0
 */
int ev_q_push(ev_q *queue, size_t item_size, void *item) {
    if (queue == NULL || node == NULL) {
        return -1;
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
char *ev_q_front(ev_q *queue) {
    if (queue == NULL) {
        return NULL;
    }

    return queue->head;
}

/**
 * Implementation of "back" queue operation.
 */
char *ev_q_back(ev_q *queue) {
    if (queue == NULL) {
        return NULL;
    }

    return queue->tail;
}

/**
 * @brief ev_q_alloc Allocates memory for ev_q and initializes max_size with the given value.
 * @return Pointer to initialized ev_q structure.
 */
ev_q *ev_q_alloc(size_t max_q_size, size_t max_item_size) {
        /* allocate memory for ev_q data structure */
        queue_t *queue = (queue_t *)malloc(sizeof(queue_t));
        queue->max_q_size = max_q_size;
        queue->cur_q_size = 0;
        queue->max_item_size = max_item_size;
        queue->splitted_item = (void *)malloc(max_item_size);
        queue->buffer_size = (sizeof(size_t) + max_item_size) * max_q_size;
        queue->buffer = (void *)malloc(queue->buffer_size);
        queue->head = queue->buffer;
        queue->tail = queue->buffer;

        return queue;
}

/**
 * @brief Frees all memory allocated for the given ev_q pointer.
 */
void ev_q_free(ev_q *queue) {
    if (queue == NULL) {
        return;
    }

    free(queue->buffer);
    free(queue->splitted_item);
    free(queue);
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
