/**
 * Copyright (C) 2017  Sergey Morozov <sergey94morozov@gmail.com>
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

#ifndef CLOUDTIERING_QUEUE_H
#define CLOUDTIERING_QUEUE_H

/*******************************************************************************
* QUEUE                                                                        *
* -----                                                                        *
*                                                                              *
* The multithreaded implementation of queue data structure capable of been     *
* allocated in a shared memory region hence supporting cross-process           *
* multithreading.                                                              *
*                                                                              *
* The queue size is defined during queue initialization and cannot be          *
* changed later. Queue elements have a fixed maximum size.                     *
* Internally, queue elements are stored in a circular buffer.                  *
* All elements will occupy equal amount of memory (maximum size per element).  *
* This makes it not memory efficient for elements with significant deviation   *
* in sizes, but in return we get predictable memory allocation policy and very *
* fast operations.                                                             *
*                                                                              *
* It is safe to use this implementation in the code which supports deffered    *
* thread cancellation but functions that handle this queue data structure      *
* do not have cancellation points inside.                                      *
*******************************************************************************/

#include <pthread.h>      /* included for a pthread_mutex_t type definition */
#include <linux/limits.h>
#include <sys/types.h>    /* included for a size_t type definition */

#include "defs.h"

#define QUEUE_SHM_OBJ    "/" PROGRAM_NAME

/* a definition of a queue data structure */
typedef struct {
        /* offset in bytes of the queue's head element
           starting from the queue pointer */
        size_t  head_offset;

        /* offset in bytes of the queue's tail element
           starting from the queue pointer */
        size_t  tail_offset;

        /* a current queue's size */
        size_t cur_size;

        /* a maximum queue's size */
        size_t max_size;

        /* a data's maximum size */
        size_t data_max_size;

        /* offset in bytes of the queue's circular buffer
           starting from the queue pointer */
        size_t buf_offset;

        /* a size in byte of a buffer where elements are stored */
        size_t buf_size;

        /* a string storing name of shared memory object
          where this queue resides */
        char shm_obj[NAME_MAX + 1];

        /* a total size in bytes of the shared memory object
           where this queue resides */
        size_t total_size;

        /* a mutex used to sequentionalize queue readers/consumers */
        pthread_mutex_t head_mutex;

        /* a mutex used to sequentionalize queue writers/suppliers */
        pthread_mutex_t tail_mutex;

        /* a mutex used to sequentionalize updates to the queue size variable
           used by readers and writers (consumers and suppliers) */
        pthread_mutex_t size_mutex;

        /* conditional variable used to indicate that the queue is capable
           to accommodate at least one element */
        pthread_cond_t fullness_cond;

        /* conditional variable used to indicate that the queue contains at
           least one element */
        pthread_cond_t emptiness_cond;
} queue_t;

/* functions to work with queue_t data structure */

/**
 * @brief queue_init Allocates memory for a queue_t data structure and
 *        initializes its members.
 *
 * @warning This function is not thread-safe with respect to queue_p parameter.
 *          Memory leaks are possible if used thoughtless in multithreaded code.
 *
 * @param[out] queue_p        A pointer to the queue to be initialized with
 *                            allocated memory region.
 * @param[in]  queue_max_size A maximum size of the queue in elements.
 * @param[in]  data_max_size  A maximum size of one element.
 * @param[in]  shm_obj        A name of shared memory object to be created.
 *                            If NULL, then queue will be created in
 *                            process-private memory.
 *
 * @return  0: queue has been initialized;
 *         -1: queue has not been initialized.
 */
int queue_init(queue_t **queue_p,
               size_t queue_max_size,
               size_t data_max_size,
               const char *shm_obj);

/**
 * @brief queue_destroy Frees all memory allocated for a given queue.
 *
 * @warning This function is not thread-safe. Memory access violations are
 *          possible if one thread attempts to destroy the queue and other
 *          attempts to use queue operations. Dead locks are also possible for
 *          the same reason.
 *
 * @param[in,out] queue Pointer to a queue's structure to be freed.
 */
void queue_destroy(queue_t *queue);

/**
 * @brief queue_push Pushes an element into a queue. If the queue is full,
 *                   block until there is available space.
 *
 * @note This function is thread-safe.
 *
 * @param[in,out] queue     The queue into which a new element will be pushed.
 * @param[in]     data      A provided data.
 * @param[in]     data_size A size of the provided data.
 *
 * @return  0: the element pushed successfully into the queue;
 *         -1: incorrect input parameters provided.
 */
int  queue_push(queue_t *queue,
                const char *data,
                size_t data_size);

/**
 * @brief queue_try_push Pushes an element into a queue. If the queue is full,
 *                       returns immediatelly with error status.
 *
 * @note This function is thread-safe.
 *
 * @param[in,out] queue     The queue into which a new element will be pushed.
 * @param[in]     data      A provided data.
 * @param[in]     data_size A size of the provided data.
 *
 * @return  0: the element pushed successfully into the queue;
 *         -1: incorrect input parameters provided or there is no free space.
 */
int  queue_try_push(queue_t *queue,
                    const char *data,
                    size_t data_size);

/**
 * @brief queue_pop Fills provided buffers for a data and a data's size
 *                  with the front queue element's data and size
 *                  correspondingly and then removes this element from the
 *                  queue. If the queue is empty, blocks until there is
 *                  available element.
 *
 * @note This function is thread-safe.
 *
 * @param[in,out] queue     The queue whose front element will be
 *                          returned and removed.
 * @param[out]    data      Pointer to a buffer of an appropriate size.
 * @param[in,out] data_size Pointer to a buffer where the data's size
 *                          will be written.
 *
 * @return  0: data has been writtem to provided buffer, data size pointer
 *             updated and element removed from the queue;
 *         -1: incorrect input parameters provided;
 */
int  queue_pop(queue_t *queue,
               char *data,
               size_t *data_size);

/**
 * @brief queue_try_pop Fills provided buffers for a data and a data's size
 *                      with the front queue element's data and size
 *                      correspondingly and then removes this element from the
 *                      queue. If the queue is empty, return immediatelly with
 *                      a error.
 *
 * @note This function is thread-safe.
 *
 * @param[in,out]  queue    The queue whose front element will be
 *                          returned and removed.
 * @param[out] data         Pointer to a buffer of an appropriate size.
 * @param[in,out] data_size Pointer to a buffer where the data's size
 *                          will be written.
 *
 * @return  0: data has been writtem to provided buffer, data size pointer
 *             updated and element removed from the queue;
 *         -1: incorrect input parameters provided or queue is empty.
 */
int  queue_try_pop(queue_t *queue,
                   char *data,
                   size_t *data_size);

#endif /* CLOUDTIERING_QUEUE_H */
