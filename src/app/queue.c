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
#define  _BSD_SOURCE        /* needed for MAP_ANONYMOUS from sys/mman.h */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>          /* defines O_* constants */
#include <sys/stat.h>       /* defines mode constants */
#include <sys/mman.h>
#include <sys/types.h>

#include "cloudtiering.h"


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
static inline size_t queue_bytes_per_elem(const queue_t *queue) {
        return (sizeof(size_t) + queue->data_max_size);
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
static inline const char *queue_buf_end(const queue_t *queue) {
        return (queue->buf + queue->buf_size);
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
static inline size_t queue_elem_size(const char *elem) {
        return (*((size_t *)elem));
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
static inline char *queue_elem_data(const char *elem) {
        return ((char *)elem + sizeof(size_t));
}

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

        /* a begining and an end of a buffer are logically identical */
        char *ptr = (queue->tail == queue_buf_end(queue)) ?
                                (char *)queue->buf : (char *)queue->tail;

        /* fill an element's space in buffer with a size of the data
           and the data itself */
        memcpy(ptr, (char *)&data_size, sizeof(size_t));
        memcpy(queue_elem_data(ptr), data, data_size);

        /* update the queue's internal state */
        queue->tail = (char *)(ptr + queue_bytes_per_elem(queue));

        pthread_mutex_lock(&queue->size_mutex);

        if (queue->cur_size++ == 0) {
                pthread_cond_broadcast(&queue->emptiness_cond);
        }

        pthread_mutex_unlock(&queue->size_mutex);
        pthread_mutex_unlock(&queue->tail_mutex);

        return 0;
}

int queue_push(queue_t *queue, const char *data, size_t data_size) {
        return queue_push_common(queue, data, data_size, 1);
}

int queue_try_push(queue_t *queue, const char *data, size_t data_size) {
        return queue_push_common(queue, data, data_size, 0);
}

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

        size_t elem_sz = queue_elem_size(queue->head);

        /* handle situation when provided buffer is not big enough */
        if (*data_size < elem_sz) {
                pthread_mutex_unlock(&queue->head_mutex);
                return -1;
        }

        /* set a value of the data's size */
        *data_size = elem_sz;

        /* copy data to provided buffer */
        memcpy(data, queue_elem_data(queue->head), *data_size);

        /* a begining and an end of a buffer are logically identical;
           update the queue's internal state */
        queue->head = (queue->head ==
                       (queue_buf_end(queue) - queue_bytes_per_elem(queue))) ?
                           (char *)queue->buf :
                           (char *)queue->head + queue_bytes_per_elem(queue);

        pthread_mutex_lock(&queue->size_mutex);

        if (queue->cur_size-- == queue->max_size) {
                pthread_cond_broadcast(&queue->fullness_cond);
        }

        pthread_mutex_unlock(&queue->size_mutex);
        pthread_mutex_unlock(&queue->head_mutex);

        return 0;
}

int queue_pop(queue_t *queue,
              char *data,
              size_t *data_size) {

        return queue_pop_common(queue, data, data_size, 1);
}

int queue_try_pop(queue_t *queue,
                  char *data,
                  size_t *data_size) {

        return queue_pop_common(queue, data, data_size, 0);
}

/**
 * @brief queue_init Allocates memory for a queue_t data structure and
 *                   initializes its members.
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
                /* queue will be used only by callee */
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

                /* create shared memory object */
                int fd = shm_open(shm_obj,
                                  O_CREAT | O_EXCL | O_RDWR,
                                  S_IRUSR | S_IWUSR | S_IRGRP |
                                  S_IWGRP | S_IROTH | S_IWOTH);
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
        queue->buf = (char *)queue + queue_t_size_aligned;

        queue->head = (char *)queue->buf;
        queue->tail = (char *)queue->buf;

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
 * @brief queue_destroy Frees all memory allocated for a given queue.
 *
 * @warning This function is not thread-safe.
 *
 * @param[out] queue Pointer to a queue's structure to be freed.
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



