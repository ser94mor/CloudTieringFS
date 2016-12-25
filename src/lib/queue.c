/**
 * Copyright (C) 2016  Sergey Morozov <sergey94morozov@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>

#include "cloudtiering.h"


/**
 * @brief queue_empty Indicates that the queue is empty.
 *
 * @note This function is thread-safe.
 *
 * @param[in] queue Queue to check for emptiness.
 *
 * @return  1: queue size is zero
 *          0: queue size is greater than zero
 *         -1: queue pointer equals NULL
 */
int queue_empty(queue_t *queue) {
        /* check input parameter's correctness */
        if (queue == NULL) {
                return -1;
        }

        /* check internal queue state under lock for thread safety */
        pthread_mutex_lock(&queue->mutex);

        /* ensure that value of 1 will be returned if queue is empty */
        int ret = !!(queue->cur_size == 0);

        pthread_mutex_unlock(&queue->mutex);

        return ret;
}


/**
 * @brief queue_full Indicates that the queue is full.
 *
 * @note This function is thread-safe.
 *
 * @param[in] queue Queue to check for fullness.
 *
 * @return  1: queue size reached its maximum
 *          0: queue size is less than its maximum
 *         -1: queue pointer equals NULL
 */
int queue_full(queue_t *queue) {
        /* check input parameter's correctness */
        if (queue == NULL) {
                return -1;
        }

        /* check internal queue state under lock for thread safety */
        pthread_mutex_lock(&queue->mutex);

        /* ensure that value of 1 will be returned if queue is full */
        int ret = !!(queue->cur_size == queue->max_size);

        pthread_mutex_unlock(&queue->mutex);

        return ret;
}


/**
 * @brief queue_contains Checks the presence of element in the queue.
 *
 * @note This function is not thread-safe.
 * @note This function does not check correctness of input parameters.
 *
 * @param[in] queue     Queue where an attempt to find an element
 *                      will be performed.
 * @param[in] elem      Element which is a target of searching.
 * @param[in] elem_size Element's size in bytes.
 *
 * @return  1: if element found in queue
 *         -1: if element not found in queue
 */
static int queue_contains(queue_t *queue, const char *elem, size_t elem_size) {
        /* pointer to the currently considered element of queue */
        char *q_ptr = queue->head;

        /* number of unconsidered queue elements */
        size_t sz = queue->cur_size;

        while (sz != 0) {
                if (q_ptr == (queue->buf + queue->buf_size)) {
                        q_ptr = queue->buf;
                }

                if ((*((size_t *)q_ptr) == elem_size) &&
                    memcmp(q_ptr + sizeof(size_t), elem, elem_size)) {
                        return 1; /* true */
                }

                q_ptr += sizeof(size_t) + queue->elem_max_size;
                --sz;
        }

        return 0; /* false */
}

/**
 * @brief queue_push Push item into queue if there is enough place for this item.
 * @return 0 on sussess; -1 on failure
 */
int queue_push(queue_t *queue, const char *item, size_t item_size) {
        if (queue == NULL || item == NULL || item_size == 0 ||
            item_size > queue->elem_max_size) {
                return -1;
        }

        pthread_mutex_lock(&queue->mutex);

        if (queue->cur_size == queue->max_size) {
                pthread_mutex_unlock(&queue->mutex);
                return -1;
        }

        if (queue_contains(queue, item, item_size)) {
                pthread_mutex_unlock(&queue->mutex);
                return -1;
        }

        char *ptr = (queue->tail == (queue->buf + queue->buf_size)) ?
                                       queue->buf : queue->tail;

        memcpy(ptr, (char *)&item_size, sizeof(size_t));
        memcpy(ptr + sizeof(size_t), item, item_size);

        queue->tail = (char *)(ptr + sizeof(size_t) + queue->elem_max_size);
        queue->cur_size++;

        pthread_mutex_unlock(&queue->mutex);

        return 0;
}


/**
 * @brief queue_pop Pop item from queue if queue is not empty.
 * @return 0 it item was popped; -1 if queue is NULL or queue is empty
 */
int queue_pop(queue_t *queue) {
        if (queue == NULL) {
                return -1;
        }

        pthread_mutex_lock(&queue->mutex);

        if (queue->cur_size == 0) {
                pthread_mutex_unlock(&queue->mutex);
                return -1;
        }

        queue->head =
        (queue->head == (queue->buf + queue->buf_size - queue->elem_max_size - sizeof(size_t))) ?
                          queue->buf :
                          queue->head + queue->elem_max_size + sizeof(size_t);
        queue->cur_size--;

        pthread_mutex_unlock(&queue->mutex);

        return 0;
}

/**
 * @brief queue_front Returns pointer to head queue element and updates size of element varaible.
 * @return Pointer to head queue element or NULL if there are no elements.
 */
const char *queue_front(queue_t *queue, size_t *size) {
        if (queue == NULL || size == NULL) {
                return NULL;
        }

        pthread_mutex_lock(&queue->mutex);

        if (queue->cur_size == 0) {
                pthread_mutex_unlock(&queue->mutex);
                return NULL;
        }

        *size = (size_t)(*queue->head);
        char *ret = queue->head + sizeof(size_t);

        pthread_mutex_unlock(&queue->mutex);

        return ret;
}


/**
 * @brief queue_alloc Allocates memory for a queue and initializes max_q_size with the given value.
 * @return Pointer to initialized queue_t.
 */
queue_t *queue_alloc(size_t max_q_size, size_t max_item_size) {
        /* allocate memory for queue_t data structure */
        queue_t *queue = malloc(sizeof(queue_t));
        if (queue == NULL) {
                return NULL;
        }

        /* initialize structure members */
        queue->buf_size = (sizeof(size_t) + max_item_size) * max_q_size;
        queue->buf = malloc(queue->buf_size);
        if (queue->buf == NULL) {
                free(queue);
                return NULL;
        }
        queue->max_size = max_q_size;
        queue->cur_size = 0;
        queue->elem_max_size = max_item_size;
        queue->head = queue->buf;
        queue->tail = queue->buf;

        queue->mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;

        return queue;
}


/**
 * @brief queue_free Frees all memory allocated for the given queue.
 */
void queue_free(queue_t *queue) {
        if (queue == NULL) {
                return;
        }

        free(queue->buf);
        free(queue);
}
