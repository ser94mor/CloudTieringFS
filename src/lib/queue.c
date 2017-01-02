/**
 * Copyright (C) 2016, 2017  Sergey Morozov <sergey94morozov@gmail.com>
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
 * @brief queue_bytes_per_elem Returns a number of bytes consumed by
 *                             one queue's element.
 *
 * @note This function is thread-safe.
 * @note This function does not check a correctness of input parameters.
 *
 * @param[in] queue A queue whose bytes-per-element value will be calculated.
 *
 * @return a number of bytes required to store one element in a queue
 */
static inline size_t queue_bytes_per_elem(const queue_t *queue) {
        return (sizeof(size_t) + queue->data_max_size);
}


/**
 * @brief queue_buf_end Returns a pointer to queue's buffer end.
 *
 * @note This function is thread-safe.
 * @note This function does not check a correctness of input parameters.
 *
 * @param[in] queue A queue whose end of buffer pointer will be returned.
 *
 * @return a pointer to an end of queue's buffer
 */
static inline const char *queue_buf_end(const queue_t *queue) {
        return (queue->buf + queue->buf_size);
}


/**
 * @brief queue_elem_size Returns a size in bytes of a given element of a queue.
 *
 * @note This function in not thread-safe.
 * @note This function does not check a correctness of input parameters.
 *
 * @param[in] elem A pointer to an element's space in a queue's buffer.
 *
 * @return size of a data of a given element
 */
static inline size_t queue_elem_size(const char *elem) {
        return (*((size_t *)elem));
}


/**
 * @brief queue_elem_data A pointer to an element's data.
 *
 * @note This function in not thread-safe.
 * @note This function does not check a correctness of input parameters.
 *
 * @param[in] elem A pointer to an element's space in a queue's buffer.
 *
 * @return a pointer to an element's data
 */
static inline char *queue_elem_data(const char *elem) {
        return ((char *)elem + sizeof(size_t));
}


/**
 * @brief queue_contains Checks a presence of a given data in a queue.
 *
 * @note This function is not thread-safe.
 * @note This function does not check a correctness of input parameters.
 *
 * @param[in] queue     A queue where an attempt to find an element
 *                      will be performed.
 * @param[in] data      A data which is a target of searching.
 * @param[in] data_size A data's size in bytes.
 *
 * @return  1: if a data found in a queue
 *         -1: if a data not found in a queue
 */
static int queue_contains(queue_t *queue, const char *data, size_t data_size) {
        /* a pointer to a currently considered element of a queue */
        char *q_ptr = queue->head;

        /* a number of unconsidered queue's elements */
        size_t sz = queue->cur_size;

        while (sz != 0) {
                /* a begining and an end of a buffer are logically identical */
                if (q_ptr == queue_buf_end(queue)) {
                        q_ptr = (char *)queue->buf;
                }

                /* found if an element's data equals to a given data */
                if ((queue_elem_size(q_ptr) == data_size) &&
                        memcmp(queue_elem_data(q_ptr), data, data_size)) {
                        return 1; /* true */
                        }

                        /* go to a next element */
                        q_ptr += queue_bytes_per_elem(queue);
                --sz;
        }

        return 0; /* false */
}


/**
 * @brief queue_empty Indicates that a queue is empty.
 *
 * @note This function is thread-safe.
 *
 * @param[in] queue A queue to check for emptiness.
 *
 * @return  1: a queue's size is zero
 *          0: a queue's size is greater than zero
 *         -1: a queue's pointer equals NULL
 */
int queue_empty(queue_t *queue) {
        /* check an input parameter's correctness */
        if (queue == NULL) {
                return -1;
        }

        /* check an internal queue's state under a lock for thread-safety */
        pthread_mutex_lock(&queue->mutex);

        /* ensure that a value of 1 will be returned if a queue is empty */
        int ret = !!(queue->cur_size == 0);

        pthread_mutex_unlock(&queue->mutex);

        return ret;
}


/**
 * @brief queue_full Indicates that a queue is full.
 *
 * @note This function is thread-safe.
 *
 * @param[in] queue A queue to check for fullness.
 *
 * @return  1: a queue's size reached its maximum
 *          0: a queue's size is less than its maximum
 *         -1: a queue's pointer equals NULL
 */
int queue_full(queue_t *queue) {
        /* check an input parameter's correctness */
        if (queue == NULL) {
                return -1;
        }

        /* check an internal queue's state under a lock for thread-safety */
        pthread_mutex_lock(&queue->mutex);

        /* ensure that a value of 1 will be returned if a queue is full */
        int ret = !!(queue->cur_size == queue->max_size);

        pthread_mutex_unlock(&queue->mutex);

        return ret;
}


/**
 * @brief queue_push Pushes an element into a queue if the queue is not full.
 *
 * @note This function is thread-safe.
 *
 * @param[in,out] queue     The queue into which a new element will be pushed.
 * @param[in]     data      A provided data.
 * @param[in]     data_size A size of the provided data.
 *
 * @return  0: the element pushed successfully into the queue
 *         -1: incorrect input parameters provided or the queue is full
 */
int queue_push(queue_t *queue, const char *data, size_t data_size) {
        /* check an input parameters' correctness */
        if (queue == NULL || data == NULL || data_size == 0 ||
            data_size > queue->data_max_size) {
                return -1;
        }

        /* modify an internal queue's state under a lock for thread-safety */
        pthread_mutex_lock(&queue->mutex);

        /* fail if the queue is full */
        if (queue->cur_size == queue->max_size) {
                pthread_mutex_unlock(&queue->mutex);
                return -1;
        }

        /* fail if the data is already presented in the queue */
        if (queue_contains(queue, data, data_size)) {
                pthread_mutex_unlock(&queue->mutex);
                return -1;
        }

        /* a begining and an end of a buffer are logically identical */
        char *ptr = (queue->tail == queue_buf_end(queue)) ?
                                (char *)queue->buf : (char *)queue->tail;

        /* fill an element's space in buffer with a size of the data
           and the data itself */
        memcpy(ptr, (char *)&data_size, sizeof(size_t));
        memcpy(queue_elem_data(ptr), data, data_size);

        /* update the queue's internal state */
        queue->tail = (char *)(ptr + queue_bytes_per_elem(queue));
        queue->cur_size++;

        pthread_mutex_unlock(&queue->mutex);

        return 0;
}


/**
 * @brief queue_pop Pops an element from a queue if the queue is not empty.
 *
 * @note This function is thread-safe.
 *
 * @param[in,out] queue The queue from where the head's element will be popped.
 *
 * @return  0: the element was popped successfully
 *         -1: incorrect input parameters provided or the queue is empty
 */
int queue_pop(queue_t *queue) {
        /* check an input parameter's correctness */
        if (queue == NULL) {
                return -1;
        }

        /* modify an internal queue's state under a lock for thread-safety */
        pthread_mutex_lock(&queue->mutex);

        /* fail if there is nothing to pop */
        if (queue->cur_size == 0) {
                pthread_mutex_unlock(&queue->mutex);
                return -1;
        }

        /* a begining and an end of a buffer are logically identical;
           update the queue's internal state */
        queue->head = (queue->head ==
                       (queue_buf_end(queue) - queue_bytes_per_elem(queue))) ?
                           (char *)queue->buf :
                           (char *)queue->head + queue_bytes_per_elem(queue);
        queue->cur_size--;

        pthread_mutex_unlock(&queue->mutex);

        return 0;
}


/**
 * @brief queue_front Fills provided buffers for a data and a data's size
 *                    with front queue element's values. Can be used with
 *                    a NULL data pointer to obtain only the size of the front
 *                    queue's element.
 *
 * @note This function is thread-safe.
 *
 * @param[in]  queue     The queue whose front element will be processed.
 * @param[out] data      Pointer to a buffer of an appropriate size or NULL.
 * @param[out] data_size Pointer to a buffer where the data's size
 *                       will be written.
 *
 * @return  0: the data_size parameter set and, if not NULL,
 *             a data buffer filled
 *         -1: incorrect input parameters provided or the queue is empty
 */
int queue_front(queue_t *queue, char *data, size_t *data_size) {
        /* check an input parameters' correctness; a data can be NULL */
        if (queue == NULL || data_size == NULL) {
                return -1;
        }

        /* access an internal queue's state under a lock for thread-safety */
        pthread_mutex_lock(&queue->mutex);

        /* fail if the queue is empty */
        if (queue->cur_size == 0) {
                pthread_mutex_unlock(&queue->mutex);
                return -1;
        }

        /* set a value of the data's size */
        *data_size = queue_elem_size(queue->head);

        /* set data only in case a buffer for data provided */
        if (data != NULL) {
                memcpy(data, queue_elem_data(queue->head), *data_size);
        }

        pthread_mutex_unlock(&queue->mutex);

        return 0;
}


/**
 * @brief queue_alloc Allocates memory for a queue_t data structure and
 *                    initializes its members.
 *
 * @note This function is thread-safe.
 *
 * @param[in] max_size      A maximum number of elements in a queue.
 * @param[in] data_max_size A maximum size of a data allowed to be pushed
 *                          into the queue.
 *
 * @return Pointer to initialized queue_t.
 */
queue_t *queue_alloc(size_t max_size, size_t data_max_size) {
        /* allocate a memory for a queue_t data structure */
        queue_t *queue = malloc(sizeof(queue_t));
        if (queue == NULL) {
                return NULL;
        }

        /* initialize structure members */
        queue->max_size = max_size;
        queue->cur_size = 0;
        queue->data_max_size = data_max_size;

        queue->buf_size = (sizeof(size_t) + data_max_size) * max_size;
        queue->buf = malloc(queue->buf_size);
        if (queue->buf == NULL) {
                free(queue);
                return NULL;
        }

        queue->head = (char *)queue->buf;
        queue->tail = (char *)queue->buf;

        queue->mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;

        return queue;
}


/**
 * @brief queue_free Frees all memory allocated for a given queue.
 *
 * @note This function is not thread-safe.
 *
 * @param[out] queue Pointer to a queue's structure to be freed.
 */
void queue_free(queue_t *queue) {
        if (queue == NULL) {
                return;
        }

        free((char *)queue->buf);
        free(queue);
}
