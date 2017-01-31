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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>

#include "cloudtiering.h"

int pqueue_enqueue(pqueue_t *pqueue,
                   const char *data,
                   size_t data_size,
                   enum data_priority_enum data_type) {
        /* check an input parameters' correctness */
        if (pqueue == NULL || data == NULL || data_size == 0) {
                return -1;
        }

        int ret = -1;

        switch(data_type) {
                case primary_e:
                        ret = queue_enqueue((queue_t *)pqueue->primary_queue,
                                         data,
                                         data_size);
                        break;
                case secondary_e:
                        ret = queue_enqueue((queue_t *)pqueue->secondary_queue,
                                         data,
                                         data_size);
                        break;
                default:
                        ret = -1;
        }

        return ret;
}

int pqueue_dequeue(pqueue_t *pqueue) {
        /* check an input parameter's correctness */
        if (pqueue == NULL) {
                return -1;
        }

        pthread_mutex_lock(&pqueue->mutex);

        int ret = queue_dequeue((queue_t *)pqueue->primary_queue);
        if (ret == -1) {
                ret = queue_dequeue((queue_t *)pqueue->secondary_queue);
        }

        pthread_mutex_unlock(&pqueue->mutex);

        return ret;
}


int pqueue_dequeue(pqueue_t *pqueue, char *data, size_t *data_size) {
        /* check an input parameters' correctness; a data can be NULL */
        if (pqueue == NULL || data_size == NULL) {
                return -1;
        }

        int ret = queue_front((queue_t *)pqueue->prio_queue,
                              data,
                              data_size);
        if (ret == -1) {
                ret = queue_front((queue_t *)pqueue->post_queue,
                                  data,
                                  data_size);
        }

        return ret;
}

pqueue_t *pqueue_alloc(size_t max_size, size_t data_max_size) {
        /* allocate a memory for a pqueue_t data structure */
        pqueue_t *pqueue = malloc(sizeof(pqueue_t));
        if (pqueue == NULL) {
                return NULL;
        }

        pqueue->prio_queue = queue_alloc(max_size, data_max_size);
        pqueue->post_queue = queue_alloc(max_size, data_max_size);
        if (pqueue->prio_queue == NULL || pqueue->post_queue == NULL) {
                free((queue_t *)pqueue->prio_queue);
                free((queue_t *)pqueue->post_queue);
                free(pqueue);
                return NULL;
        }

        pqueue->mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;

        return pqueue;
}

void pqueue_free(pqueue_t *pqueue) {
        if (pqueue == NULL) {
                return;
        }

        free((queue_t *)pqueue->prio_queue);
        free((queue_t *)pqueue->post_queue);
        free(pqueue);

        return;
}
