#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>

#include "cloudtiering.h"


/**
 * @brief queue_empty Indicates that queue is empty.
 * @return >0 if size equals 0; 0 if size greater then 0; -1 if queue equals NULL
 */
int queue_empty(queue_t *queue) {
        if (queue == NULL) {
                return -1;
        }
        
        pthread_mutex_lock(&queue->mutex);
        
        int ret = (queue->cur_q_size == 0);
        
        pthread_mutex_unlock(&queue->mutex);
        
        return ret;
}


/**
 * @brief queue_full Indicates that the queue is full.
 * @return >0 if size equals maximem queue size therwise 0; -1 if queue equals NULL
 */
int queue_full(queue_t *queue) {
        if (queue == NULL) {
                return -1;
        }
        
        pthread_mutex_lock(&queue->mutex);
        
        int ret = (queue->cur_q_size == queue->max_q_size);
        
        pthread_mutex_unlock(&queue->mutex);
        
        return ret;
}


/**
 * @brief queue_push Push item into queue if there is enough place for this item.
 * @return 0 on sussess; -1 on failure
 */
int queue_push(queue_t *queue, char *item, size_t item_size) {
        if (queue == NULL || item == NULL || item_size == 0 ||
            item_size > queue->max_item_size) {
                return -1;
        }

        pthread_mutex_lock(&queue->mutex);
        
        if (queue->cur_q_size == queue->max_q_size) {
                pthread_mutex_unlock(&queue->mutex);
                return -1;
        }
        
        char *ptr = queue->tail == (queue->buffer + queue->buffer_size) ? queue->buffer : queue->tail;

        memcpy(ptr, (char *)&item_size, sizeof(size_t));
        memcpy(ptr + sizeof(size_t), item, item_size);

        queue->tail = (char *)(ptr + sizeof(size_t) + queue->max_item_size);
        queue->cur_q_size++;
        
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
        
        if (queue->cur_q_size == 0) {
                pthread_mutex_unlock(&queue->mutex);
                return -1;
        }

        queue->head = queue->head == (queue->buffer + queue->buffer_size - queue->max_item_size - sizeof(size_t)) ?
                          queue->buffer :
                          queue->head + queue->max_item_size + sizeof(size_t);
        queue->cur_q_size--;
        
        pthread_mutex_unlock(&queue->mutex);

        return 0;
}

/**
 * @brief queue_front Returns pointer to head queue element and updates size of element varaible.
 * @return Pointer to head queue element or NULL if there are no elements.
 */
char *queue_front(queue_t *queue, size_t *size) {
        if (queue == NULL || size == NULL) {
                if (size != NULL) {
                        *size = 0;
                }
                
                return NULL;
        }
        
        pthread_mutex_lock(&queue->mutex);
        
        if (queue->cur_q_size == 0) {
                pthread_mutex_unlock(&queue->mutex);
                *size = 0;
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
        queue_t *queue = (queue_t *)malloc(sizeof(queue_t));
        if (queue == NULL) {
                return NULL;
        }
        
        /* initialize structure members */
        queue->buffer = (char *)malloc(queue->buffer_size);
        if (queue->buffer == NULL) {
                return NULL;
        }
        queue->max_q_size = max_q_size;
        queue->cur_q_size = 0;
        queue->max_item_size = max_item_size;
        queue->buffer_size = (sizeof(size_t) + max_item_size) * max_q_size;
        queue->head = queue->buffer;
        queue->tail = queue->buffer;
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

    free(queue->buffer);
    free(queue);
}


/**
 * @brief queue_print Prints/Writes string representation of queue to the given file stream.
 * @note Use this function for debug only.
 * @return 0 on success and -1 on failure.
 */
int queue_print(FILE *stream, queue_t *queue) {
        if (queue == NULL || stream == NULL) {
                return -1;
        }
        
        pthread_mutex_lock(&queue->mutex);
        
        char *q_ptr = queue->head;

        char buf[queue->max_item_size + 1];

        /* header */
        fprintf(stream, "QUEUE:\n");

        /* metadata */
        fprintf(stream, "\t< cur. queue size : %zu >\n"\
                        "\t< max. queue size : %zu >\n"\
                        "\t< max. item  size : %zu >\n"\
                        "\t< queue buf. size : %zu >\n",
                queue->cur_q_size, queue->max_q_size,
                queue->max_item_size, queue->buffer_size);

        /* data */
        size_t sz = queue->cur_q_size;
        while (sz != 0) {
                if (q_ptr == (queue->buffer + queue->buffer_size)) {
                        q_ptr = queue->buffer;
                }
                memcpy(buf, q_ptr + sizeof(size_t), (size_t)(*q_ptr));
                buf[(size_t)(*q_ptr)] = '\0';
                fprintf(stream, "\t|--> %zu %s \n", (size_t)(*q_ptr), buf);
                q_ptr += sizeof(size_t) + queue->max_item_size;
                --sz;
        }

        fflush(stream);
        
        pthread_mutex_unlock(&queue->mutex);
        
        return 0;
}
