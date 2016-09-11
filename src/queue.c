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
 * @note TODO: use mutex syncronization !!!
 * @return 0 on sussess; <0 on failure
 */
int queue_push(queue_t *queue, void *item, size_t item_size) {
        if (queue == NULL || item == NULL || item_size == 0 ||
            item_size > queue->max_item_size ||
            queue->cur_q_size == queue->max_q_size) {
                return -1;
        }

        /* to maximize speed use very greedy memory policy */
        void *ptr = queue->tail == (queue->buffer + queue->buffer_size) ? queue->buffer : queue->tail;
        memcpy(ptr, &item_size, sizeof(size_t));
        memcpy(ptr + sizeof(size_t), item, item_size);

        queue->tail = ptr + sizeof(size_t) + queue->max_item_size;
        queue->cur_q_size++;

        return 0;
}

/**
 * @brief queue_pop Pop item from queue if queue is not empty.
 * @note TODO: use mutex syncronization !!!
 * @return 0 it item was popped; <0 if queue is NULL or queue is empty
 */
int queue_pop(ev_q *queue) {
        if (queue == NULL || queue->cur_q_size == 0) {
                return -1;
        }

        queue->head = queue->head == (queue->buffer + queue->buffer_size) ?
                          queue->buffer + queue->max_item_size + sizeof(size_t) :
                          queue->head + queue->max_item_size + sizeof(size_t);
        queue->cur_q_size--;

        return 0;
}

void *queue_front(queue_t *queue, size_t *size) {
        size = queue->head;
        return queue->head + sizeof(size_t);
}

/**
 * @brief queue_alloc Allocates memory for a queue and initializes max_q_size with the given value.
 * @return Pointer to initialized queue_t.
 */
queue_t *queue_alloc(size_t max_q_size, size_t max_item_size) {
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
 * @brief queue_free Frees all memory allocated for the given queue.
 */
void queue_free(queue_t *queue) {
    if (queue == NULL) {
        return;
    }

    free(queue->buffer);
    free(queue->splitted_item);
    free(queue);
}

const char *queue_str(queue_t *queue) {
        
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
