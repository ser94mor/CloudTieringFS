/**
 * Copyright (C) 2016, 2017  Sergey Morozov <sergey@morozov.ch>
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
#define  _BSD_SOURCE        /* needed for MAP_ANONYMOUS from sys/mman.h */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>          /* defines O_* constants */
#include <sys/stat.h>       /* defines mode constants */
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "queue.h"


/**
 * @brief queue_bytes_per_elem Returns a number of bytes consumed by
 *                             one queue's element.
 *
 * @note This function is thread-safe.
 * @warning This function does not check a correctness of input parameters.
 *
 * @param[in] queue A queue whose bytes-per-element value will be calculated.
 *
 * @return a number of bytes required to store one element in a queue
 */
static inline size_t queue_bytes_per_elem( const queue_t *queue ) {
        return ( sizeof( size_t ) + queue->data_max_size );
}


/**
 * @brief queue_buf_end Returns a pointer to queue's buffer end.
 *
 * @note This function is thread-safe.
 * @warning This function does not check a correctness of input parameters.
 *
 * @param[in] queue A queue whose end of buffer pointer will be returned.
 *
 * @return a pointer to an end of queue's buffer
 */
static inline const char *queue_buf_end( const queue_t *queue ) {
        return ( (char *)queue + queue->buf_offset + queue->buf_size );
}


/**
 * @brief queue_elem_size Returns a size in bytes of a given element of a queue.
 *
 * @warning This function in not thread-safe.
 * @warning This function does not check a correctness of input parameters.
 *
 * @param[in] elem A pointer to an element's space in a queue's buffer.
 *
 * @return size of a data of a given element
 */
static inline size_t queue_elem_size( const char *elem ) {
        return ( *( (size_t *)elem ) );
}


/**
 * @brief queue_elem_data A pointer to an element's data.
 *
 * @warning This function in not thread-safe.
 * @warning This function does not check a correctness of input parameters.
 *
 * @param[in] elem A pointer to an element's space in a queue's buffer.
 *
 * @return a pointer to an element's data
 */
static inline char *queue_elem_data( const char *elem ) {
        return ( (char *)elem + sizeof( size_t ) );
}


/**
 * @brief queue_buf_beg A pointer to a queue's buffer.
 *
 * @note This function is thread-safe.
 * @warning This function does not check a correctness of input parameters.
 *
 *  @param[in] queue A queue whose buffer's pointer will be returned.
 *
 * @return a pointer to queue's buffer
 */
static inline char *queue_buf_beg( const queue_t *queue ) {
        return ( (char *)queue + queue->buf_offset );
}


/**
 * @brief queue_head A pointer to a queue's head.
 *
 * @warning This function is not thread-safe.
 * @warning This function does not check a correctness of input parameters.
 *
 *  @param[in] queue A queue whose head's pointer will be returned.
 *
 * @return a pointer to queue's head
 */
static inline char *queue_head( const queue_t *queue ) {
        return ( (char *)queue + queue->head_offset );
}


/**
 * @brief queue_tail A pointer to a queue's tail.
 *
 * @warning This function is not thread-safe.
 * @warning This function does not check a correctness of input parameters.
 *
 *  @param[in] queue A queue whose tail's pointer will be returned.
 *
 * @return a pointer to queue's tail
 */
static inline char *queue_tail( const queue_t *queue ) {
        return ( (char *)queue + queue->tail_offset );
}


/**
 * @brief queue_push_common Pushes an element into a queue. The behaviour
 *                          in case of the queue full condition is determined
 *                          by a boolean parameter flag.
 *                          Common part for queue_push and queue_try_push
 *                          fucntions.
 *
 * @note This function is thread-safe.
 *
 * @param[in,out] queue       The queue into which a new element will be pushed.
 * @param[in]     data        A provided data.
 * @param[in]     data_size   A size of the provided data.
 * @param[in]     should_wait Flag defining blocking/non-blocking behaviour.
 *
 * @return  0: the element pushed successfully into the queue;
 *         -1: incorrect input parameters provided or, in case of
 *             should_wait == true, queue is full.
 */
static int queue_push_common(queue_t *queue,
                             const char *data,
                             size_t data_size,
                             int should_wait) {
        /* check an input parameters' correctness */
        if (queue == NULL || data == NULL || data_size == 0 ||
            data_size > queue->data_max_size) {
                return -1;
        }

        /* modify an internal queue's state under a lock for thread-safety */
        pthread_mutex_lock(&queue->tail_mutex);
        pthread_mutex_lock(&queue->size_mutex);

        while(queue->cur_size == queue->max_size) {
                if (!should_wait) {
                        pthread_mutex_unlock(&queue->size_mutex);
                        pthread_mutex_unlock(&queue->tail_mutex);
                        return -1;
                }
                pthread_cond_wait(&queue->fullness_cond, &queue->size_mutex);
        }

        pthread_mutex_unlock(&queue->size_mutex);

        /* a beginning and an end of a buffer are logically identical */
        queue->tail_offset = (queue->tail_offset ==
                             (queue->buf_offset + queue->buf_size)) ?
                             queue->buf_offset : queue->tail_offset;

        char *ptr = queue_tail(queue);

        /* fill an element's space in buffer with a size of the data
           and the data itself */
        memcpy(ptr, (char *)&data_size, sizeof(size_t));
        memcpy(queue_elem_data(ptr), data, data_size);

        /* update the queue's internal state */
        queue->tail_offset += queue_bytes_per_elem(queue);

        pthread_mutex_lock(&queue->size_mutex);

        if (queue->cur_size++ == 0) {
                pthread_cond_broadcast(&queue->emptiness_cond);
        }

        pthread_mutex_unlock(&queue->size_mutex);
        pthread_mutex_unlock(&queue->tail_mutex);

        return 0;
}


/**
 * Blocking queue push operation.
 * See queue.h for complete description.
 */
int queue_push(queue_t *queue, const char *data, size_t data_size) {
        return queue_push_common(queue, data, data_size, 1);
}


/**
 * Non-blocking queue push operation.
 * See queue.h for complete description.
 */
int queue_try_push(queue_t *queue, const char *data, size_t data_size) {
        return queue_push_common(queue, data, data_size, 0);
}


/**
 * @brief queue_pop_common Fills provided buffers for a data and a data's size
 *                         with the front queue element's data and size
 *                         correspondingly and then removes this element from
 *                         the queue. The behavior in case of the queue empty
 *                         condition is determined by a boolean parameter flag.
 *                         Common part for queue_pop and queue_try_pop
 *                         functions.
 *
 * @note This function is thread-safe.
 *
 * @param[in,out] queue       The queue whose front element will be
 *                            returned and removed.
 * @param[out]    data        Pointer to a buffer of an appropriate size.
 * @param[in,out] data_size   Pointer to a buffer where the data's size
 *                            will be written.
 * @param[in]     should_wait Flag defining blocking/non-blocking behavior.
 *
 * @return  0: data has been written to provided buffer, data size pointer
 *             updated and element removed from the queue;
 *         -1: incorrect input parameters provided or, in case of
 *             should_wait == true, queue is empty.
 */
static int queue_pop_common(queue_t *queue,
                            char *data,
                            size_t *data_size,
                            int should_wait) {
        /* check an input parameters' correctness; a data can be NULL */
        if (queue == NULL || data == NULL || data_size == NULL) {
                return -1;
        }

        /* modify an internal queue's state under a lock for thread-safety */
        pthread_mutex_lock(&queue->head_mutex);
        pthread_mutex_lock(&queue->size_mutex);

        while(queue->cur_size == 0) {
                if (!should_wait) {
                        pthread_mutex_unlock(&queue->size_mutex);
                        pthread_mutex_unlock(&queue->head_mutex);
                        return -1;
                }
                pthread_cond_wait(&queue->emptiness_cond, &queue->size_mutex);
        }

        pthread_mutex_unlock(&queue->size_mutex);

        size_t elem_sz = queue_elem_size(queue_head(queue));

        /* handle situation when provided buffer is not big enough */
        if (*data_size < elem_sz) {
                pthread_mutex_unlock(&queue->head_mutex);
                return -1;
        }

        /* set a value of the data size */
        *data_size = elem_sz;

        /* copy data to provided buffer */
        memcpy(data, queue_elem_data(queue_head(queue)), *data_size);

        /* a beginning and an end of a buffer are logically identical;
           update the queue's internal state */
        queue->head_offset = (queue->head_offset ==
                             (queue->buf_offset + queue->buf_size -
                             queue_bytes_per_elem(queue))) ?
                             queue->buf_offset :
                             queue->head_offset + queue_bytes_per_elem(queue);

        pthread_mutex_lock(&queue->size_mutex);

        if (queue->cur_size-- == queue->max_size) {
                pthread_cond_broadcast(&queue->fullness_cond);
        }

        pthread_mutex_unlock(&queue->size_mutex);
        pthread_mutex_unlock(&queue->head_mutex);

        return 0;
}


/**
 * Blocking queue pop operation.
 * See queue.h for complete description.
 */
int queue_pop(queue_t *queue,
              char *data,
              size_t *data_size) {

        return queue_pop_common(queue, data, data_size, 1);
}


/**
 * Non-blocking queue pop operation.
 * See queue.h for complete description.
 */
int queue_try_pop(queue_t *queue,
                  char *data,
                  size_t *data_size) {

        return queue_pop_common(queue, data, data_size, 0);
}


/**
 * Initialize queue data structure.
 * See queue.h for complete description.
 */
int queue_init(queue_t **queue_p,
               size_t max_size,
               size_t data_max_size,
               const char *shm_obj) {
        /* get page size value to properly align queue_t structure and
           queue->buf in memory */
        long val = sysconf(_SC_PAGESIZE);
        if (val == -1) {
                return -1;
        }

        size_t page_size            = (size_t)val;
        size_t queue_t_size         = sizeof(queue_t);
        size_t queue_t_size_aligned = queue_t_size + (queue_t_size % page_size);
        size_t buf_size             = (sizeof(size_t) + data_max_size) *
                                      max_size;
        size_t buf_size_aligned     = buf_size + (buf_size % page_size);

        /* total size of memory to be allocated */
        size_t total_size_aligned = queue_t_size_aligned + buf_size_aligned;

        void *mem_region = NULL;
        if (shm_obj == NULL) {
                /* queue will be used only by caller */
                mem_region = mmap(NULL,                        /* addr */
                                  total_size_aligned,          /* len */
                                  PROT_READ | PROT_WRITE,      /* prot */
                                  MAP_PRIVATE | MAP_ANONYMOUS, /* flags */
                                  -1,                          /* fd */
                                  0);                          /* offset */
                if (mem_region == MAP_FAILED) {
                        return -1;
                }
        } else {
                /* queue will be used by many processes */
                int oflags =  O_CREAT | O_EXCL | O_RDWR;
                mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP |
                              S_IWGRP | S_IROTH | S_IWOTH;
                mode_t mask = S_IXUSR | S_IXGRP | S_IXOTH;

                /* temporary set mask for the follwing shm_open call */
                mask = umask(mask);

                /* create shared memory object */
                int fd = shm_open(shm_obj, oflags, mode);

                /* restore the previous mask value */
                umask(mask);

                if (fd == -1) {
                        return -1;
                }

                /* set size equal to all memory needed for queue and
                   its buffer */
                if (ftruncate(fd, total_size_aligned) == -1) {
                        /* do not check shm_unlink() and close() errors because
                           here we are already failed */
                        close(fd);
                        shm_unlink(shm_obj);
                        return -1;
                }

                mem_region = mmap(NULL,                        /* addr */
                                  total_size_aligned,          /* len */
                                  PROT_READ | PROT_WRITE,      /* prot */
                                  MAP_SHARED,                  /* flags */
                                  fd,                          /* fd */
                                  0);                          /* offset */
                if (mem_region == MAP_FAILED) {
                        /* do not check shm_unlink() and close() errors because
                           here we are already failed */
                        close(fd);
                        shm_unlink(shm_obj);
                        return -1;
                }

                /* no longer needed */
                if (close(fd) == -1) {
                        /* close() error usually indicates serious
                           system problem */
                        munmap(mem_region, total_size_aligned);
                        shm_unlink(shm_obj);
                        return -1;
                }
        }

        queue_t *queue = mem_region;

        /* initialize structure members */
        queue->max_size = max_size;
        queue->cur_size = 0;
        queue->data_max_size = data_max_size;
        queue->total_size = total_size_aligned;

        queue->buf_size = (sizeof(size_t) + data_max_size) * max_size;
        queue->buf_offset = queue_t_size_aligned;

        queue->head_offset = queue_t_size_aligned;
        queue->tail_offset = queue_t_size_aligned;

        if (shm_obj == NULL) {
                queue->shm_obj[0] = '\0'; /* empty string */

                queue->head_mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
                queue->tail_mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
                queue->size_mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;

                queue->emptiness_cond =
                        (pthread_cond_t)PTHREAD_COND_INITIALIZER;
                queue->fullness_cond  =
                        (pthread_cond_t)PTHREAD_COND_INITIALIZER;
        } else {
                strcpy(queue->shm_obj, shm_obj);

                pthread_mutexattr_t mutex_attr;
                pthread_condattr_t  cond_attr;

                pthread_mutexattr_init(&mutex_attr);
                pthread_condattr_init(&cond_attr);

                pthread_mutexattr_setpshared(&mutex_attr,
                                             PTHREAD_PROCESS_SHARED);
                pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);

                pthread_mutex_init(&(queue->head_mutex), &mutex_attr);
                pthread_mutex_init(&(queue->tail_mutex), &mutex_attr);
                pthread_mutex_init(&(queue->size_mutex), &mutex_attr);

                pthread_cond_init(&(queue->emptiness_cond), &cond_attr);
                pthread_cond_init(&(queue->fullness_cond),  &cond_attr);
        }

        *queue_p =  queue;

        return 0;
}


/**
 * Destroy queue data structure and free resources.
 * See queue.h for complete description.
 */
void queue_destroy(queue_t *queue) {
        if (queue == NULL) {
                return;
        }

        if (strlen(queue->shm_obj) && (shm_unlink(queue->shm_obj) == -1)) {
                /* something is wrong with shared memory object */
        }

        if (munmap(queue, queue->total_size) == -1) {
                /* queue structure is corrupted */
        }
}
