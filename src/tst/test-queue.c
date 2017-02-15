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

#define _GNU_SOURCE /* needed for pthread_timedjoin_np() */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "cloudtiering.h"

#define QUEUE_MAX_SIZE    3
#define DATA_MAX_SIZE     20
#define SHM_OBJ           "/cloudtiering-shm-obj-test"

#define ITERATIONS_PER_THREAD    500000
#define COND_WAIT_SECS_THREAD    5
#define THREAD_JOIN_TIMEOUT      3
#define DATA_STR_THREAD          "data"
#define DATA_STR_LEN_THREAD      5

static char *data_arr[] = {
                "Hello, World!",
                "This is me.",
                "Let's play a game.",
                "Don't be so shy.",
};

static const char *very_long_data =
        "Hello? Hello? C-can you here me? I'm supposed to be too big, "
        "to exceed data limít.";

/* the following globals are needed for parallel access test */
static pthread_t consumer_1,
                 consumer_2,
                 supplier_1,
                 supplier_2;


static pthread_cond_t  *finish_cond;
static pthread_mutex_t *finish_mutex;
static int             *threads_finished;

static pthread_cond_t  *start_cond;
static pthread_mutex_t *start_mutex;
static int             *start_flag;

static int queue_print(FILE *stream, queue_t *queue) {
        if (queue == NULL || stream == NULL) {
                return -1;
        }

        if (pthread_mutex_trylock(&queue->head_mutex)) {
                return -1;
        }

        if (pthread_mutex_trylock(&queue->tail_mutex)) {
                pthread_mutex_unlock(&queue->head_mutex);
                return -1;
        }

        if (pthread_mutex_lock(&queue->size_mutex)) {
                pthread_mutex_unlock(&queue->tail_mutex);
                pthread_mutex_unlock(&queue->head_mutex);
                return -1;
        }

        char *q_ptr = queue->head;

        char buf[queue->data_max_size + 1];

        /* header */
        fprintf(stream, "QUEUE:\n");

        /* metadata */
        fprintf(stream, "\t< cur. queue size : %zu >\n"\
                        "\t< max. queue size : %zu >\n"\
                        "\t< max. item  size : %zu >\n"\
                        "\t< queue buf. size : %zu >\n",
                queue->cur_size, queue->max_size,
                queue->data_max_size, queue->buf_size);

        /* data */
        size_t sz = queue->cur_size;
        while (sz != 0) {
                if (q_ptr == (queue->buf + queue->buf_size)) {
                        q_ptr = (char *)queue->buf;
                }
                memcpy(buf, q_ptr + sizeof(size_t), (size_t)(*q_ptr));
                buf[(size_t)(*q_ptr)] = '\0';
                fprintf(stream, "\t|--> %zu %s \n", (size_t)(*q_ptr), buf);
                q_ptr += sizeof(size_t) + queue->data_max_size;
                --sz;
        }

        fflush(stream);

        pthread_mutex_unlock(&queue->size_mutex);
        pthread_mutex_unlock(&queue->tail_mutex);
        pthread_mutex_unlock(&queue->head_mutex);

        return 0;
}

static int test_queue_api(char *err_msg, queue_t **queue_p) {
        queue_t *queue = NULL;
        size_t data_size = DATA_MAX_SIZE;
        char data[data_size];

        if (queue_init(&queue, QUEUE_MAX_SIZE, DATA_MAX_SIZE, NULL)) {
                strcpy(err_msg, "[queue_init] should not fail with correct "
                                "input args");
                goto err;
        }

        int i;
        for (i = 0; i < QUEUE_MAX_SIZE; i++) {
                if (queue_push(queue, data_arr[i], strlen(data_arr[i]) + 1)) {
                        strcpy(err_msg, "[queue_push] should not fail with "
                                        "non-full queue");
                        goto err;
                }
        }

        /* i == QUEUE_MAX_SIZE here, i.e. index of QUEUE_MAX_SIZE + 1 data */
        if (!queue_try_push(queue, data_arr[i], strlen(data_arr[i]) + 1)) {
                strcpy(err_msg, "[queue_try_push] should had failed with full "
                                "queue but had not");
                goto err;
        }

        data_size = DATA_MAX_SIZE;
        if (queue_pop(queue, data, &data_size)) {
                strcpy(err_msg, "[queue_pop] should not fail with non-empty "
                                "queue and correct input args");
                goto err;
        }

        if (strcmp(data, data_arr[QUEUE_MAX_SIZE - i])) {
                strcpy(err_msg, "[queue_pop] returned incorrect data");
                goto err;
        }
        --i;

        /* i == QUEUE_MAX_SIZE - 1 here, i.e. index of QUEUE_MAX_SIZE data */
        if (queue_push(queue, data_arr[i], strlen(data_arr[i]) + 1)) {
                strcpy(err_msg,
                       "[queue_push] should not fail with non-full "
                       "queue (inversed order of data items)");
                goto err;
        }
        ++i;

        /* i == QUEUE_MAX_SIZE here, i.e. index of QUEUE_MAX_SIZE + 1 data */
        if (!queue_try_push(queue, data_arr[i], strlen(data_arr[i]) + 1)) {
                strcpy(err_msg, "[queue_try_push] should had failed with full "
                                "queue but had not (inversed order of "
                                "data items)");
                goto err;
        }
        --i;

        /* buffer inversion here is 1 data item; i == QUEUE_MAX_SIZE - 1 here */
        for (; i >= 1; i--) {
                data_size = DATA_MAX_SIZE;
                if (queue_pop(queue, data, &data_size)) {
                        strcpy(err_msg,
                               "[queue_pop] should not fail with non-empty "
                               "queue and correct input args (normal order of "
                               "data items)");
                        goto err;
                }

                if (strcmp(data, data_arr[QUEUE_MAX_SIZE - i])) {
                        strcpy(err_msg, "[queue_pop] returned incorrect data");
                        goto err;
                }
        }

        /* i == 0 here, i.e. index of first data item */
        data_size = DATA_MAX_SIZE;
        if (queue_pop(queue, data, &data_size)) {
                strcpy(err_msg, "[queue_pop] should not fail with non-empty "
                                "queue and correct input args (inversed order "
                                "of data items)");
                goto err;
        }

        if (strcmp(data, data_arr[QUEUE_MAX_SIZE - 1])) {
                strcpy(err_msg, "[queue_pop] returned incorrect data");
                goto err;
        }
        /* since i == 0, do not decrement it */

        data_size = DATA_MAX_SIZE;
        if (!queue_try_pop(queue, data, &data_size)) {
                strcpy(err_msg, "[queue_try_pop] should had failed for empty "
                                "queue but had not");
                goto err;
        }

        /* queue is empty; test *_try_* versions of push and pop */
        i = 0;
        while (!queue_try_push(queue, data_arr[i], strlen(data_arr[i]) + 1)) {
                ++i;
        }

        if (i != QUEUE_MAX_SIZE) {
                strcpy(err_msg, "[queue_try_push] should not have failed on "
                                "the tested iteration");
                goto err;
        }

        data_size = DATA_MAX_SIZE;
        while (!queue_try_pop(queue, data, &data_size)) {
                --i;
                data_size = DATA_MAX_SIZE;
        }

        if (i != 0) {
                strcpy(err_msg, "[queue_try_pop] should not have failed on "
                                "the tested iteration");
                goto err;
        }

        /* testing too long data item */
        if (!queue_push(queue, very_long_data, strlen(very_long_data))) {
                strcpy(err_msg, "[queue_push] should had failed for too "
                                "long data item but had not");
                goto err;
        }

        queue_destroy(queue);
        *queue_p = NULL;

        /* success */
        return 0;

    err:
        /* failure */
        *queue_p = queue;
        return -1;
}

static void *consumer_routine(void *args) {
        pthread_setcanceltype(PTHREAD_CANCEL_ENABLE, NULL);
        pthread_setcancelstate(PTHREAD_CANCEL_DEFERRED, NULL);

        queue_t *queue = (queue_t *)args;

        pthread_mutex_lock(start_mutex);
        while (!(*start_flag)) {
                pthread_cond_wait(start_cond, start_mutex);
        }
        pthread_mutex_unlock(start_mutex);

        char data[DATA_STR_LEN_THREAD];
        size_t data_size = DATA_STR_LEN_THREAD;

        for (int i = 0; i < ITERATIONS_PER_THREAD; i++) {
                if (queue_pop(queue, data, &data_size)) {
                        return "queue_pop failed";
                }
                data_size = DATA_STR_LEN_THREAD;
                pthread_testcancel();
        }

        pthread_mutex_lock(finish_mutex);
        (*threads_finished)++;
        pthread_cond_signal(finish_cond);
        pthread_mutex_unlock(finish_mutex);

        return NULL;
}

static void *supplier_routine(void *args) {
        pthread_setcanceltype(PTHREAD_CANCEL_ENABLE, NULL);
        pthread_setcancelstate(PTHREAD_CANCEL_DEFERRED, NULL);

        queue_t *queue = (queue_t *)args;

        pthread_mutex_lock(start_mutex);
        while (!(*start_flag)) {
                pthread_cond_wait(start_cond, start_mutex);
        }
        pthread_mutex_unlock(start_mutex);

        for (int i = 0; i < ITERATIONS_PER_THREAD; i++) {
                if (queue_push(queue, DATA_STR_THREAD, DATA_STR_LEN_THREAD)) {
                        return "queue_push failed";
                }
                pthread_testcancel();
        }

        pthread_mutex_lock(finish_mutex);
        (*threads_finished)++;
        pthread_cond_signal(finish_cond);
        pthread_mutex_unlock(finish_mutex);

        return NULL;
}

static int start(pthread_t *thread_id,
                 void *(action)(void *),
                 queue_t *queue,
                 const char *shm_obj,
                 char *err_msg,
                 queue_t **queue_p,
                 const char *human_name) {
        if (shm_obj == NULL) {
                if (pthread_create(thread_id, NULL, action, queue)) {
                        sprintf(err_msg,
                                "[pthread_create] failed for %s", human_name);
                        *queue_p = NULL;
                        queue_destroy(queue);
                        return -1;
                }
        } else {
                if (!fork()) {
                        int fd = shm_open(shm_obj, O_RDWR, 0);
                        if (fd == -1) {
                                exit(1);
                        }

                        struct stat sb;
                        if (fstat(fd, &sb) == -1) {
                                exit(1);
                        }

                        queue = mmap(NULL,
                                     sb.st_size,
                                     PROT_READ,
                                     MAP_SHARED,
                                     fd,
                                     0);
                        if (queue == MAP_FAILED) {
                                exit(1);
                        }

                        const char *ret_msg = action(queue);

                        if (ret_msg == NULL) {
                                exit(0);
                        } else {
                                exit(1);
                        }
                }
        }

        return 0;
}

static int prepare_pthread_stuff(const char *shm_obj) {
        pthread_mutexattr_t mutex_attr;
        pthread_condattr_t  cond_attr;

        int pshared = (shm_obj == NULL) ?
                      PTHREAD_PROCESS_PRIVATE : PTHREAD_PROCESS_SHARED;

        int flags = (shm_obj == NULL) ?
                    MAP_PRIVATE | MAP_ANONYMOUS : MAP_SHARED;

        pthread_mutexattr_init(&mutex_attr);
        pthread_condattr_init(&cond_attr);

        pthread_mutexattr_setpshared(&mutex_attr, pshared);
        pthread_condattr_setpshared(&cond_attr, pshared);

        finish_cond = mmap(NULL,
                           sizeof(pthread_cond_t),
                           PROT_READ | PROT_WRITE,
                           flags,
                           -1,
                           0);
        if (finish_cond == MAP_FAILED) {
                return -1;
        } else {
                pthread_cond_init(finish_cond, &cond_attr);
        }

        finish_mutex = mmap(NULL,
                            sizeof(pthread_mutex_t),
                            PROT_READ | PROT_WRITE,
                            flags,
                            -1,
                            0);
        if (finish_mutex == MAP_FAILED) {
                munmap(finish_cond, sizeof(pthread_cond_t));
                return -1;
        } else {
                pthread_mutex_init(finish_mutex, &mutex_attr);
        }

        threads_finished = mmap(NULL,
                                sizeof(int),
                                PROT_READ | PROT_WRITE,
                                flags,
                                -1,
                                0);
        if (threads_finished == MAP_FAILED) {
                munmap(finish_cond, sizeof(pthread_cond_t));
                munmap(finish_mutex, sizeof(pthread_mutex_t));
                return -1;
        } else {
                *threads_finished = 0;
        }

        start_cond = mmap(NULL,
                          sizeof(pthread_cond_t),
                          PROT_READ | PROT_WRITE,
                          flags,
                          -1,
                          0);
        if (start_cond == MAP_FAILED) {
                munmap(finish_cond, sizeof(finish_cond));
                munmap(finish_mutex, sizeof(finish_mutex));
                munmap(threads_finished, sizeof(threads_finished));
                return -1;
        } else {
                pthread_cond_init(start_cond, &cond_attr);
        }

        start_mutex = mmap(NULL,
                           sizeof(pthread_mutex_t),
                           PROT_READ | PROT_WRITE,
                           flags,
                           -1,
                           0);
        if (start_mutex == MAP_FAILED) {
                munmap(finish_cond, sizeof(finish_cond));
                munmap(finish_mutex, sizeof(finish_mutex));
                munmap(threads_finished, sizeof(threads_finished));
                munmap(start_cond, sizeof(start_cond));
                return -1;
        } else {
                pthread_mutex_init(start_mutex, &mutex_attr);
        }

        start_flag = mmap(NULL,
                          sizeof(int),
                          PROT_READ | PROT_WRITE,
                          flags,
                          -1,
                          0);
        if (start_flag == MAP_FAILED) {
                munmap(finish_cond, sizeof(finish_cond));
                munmap(finish_mutex, sizeof(finish_mutex));
                munmap(threads_finished, sizeof(threads_finished));
                munmap(start_cond, sizeof(start_cond));
                munmap(start_mutex, sizeof(start_mutex));
                return -1;
        } else {
                *start_flag = 0;
        }

        return 0;
}

static void destroy_pthread_stuff(void) {
        munmap(finish_cond, sizeof(finish_cond));
        munmap(finish_mutex, sizeof(finish_mutex));
        munmap(threads_finished, sizeof(threads_finished));
        munmap(start_cond, sizeof(start_cond));
        munmap(start_mutex, sizeof(start_mutex));
        munmap(start_flag, sizeof(start_flag));
}

static int test_queue_deadlock(char *err_msg,
                               queue_t **queue_p,
                               const char *shm_obj) {
        queue_t *queue = NULL;

        if (prepare_pthread_stuff(shm_obj) == -1) {
                strcpy(err_msg, "[prepare_pthread_stuff] failed");
                *queue_p = NULL;
                return -1;
        }

        if (queue_init(&queue, QUEUE_MAX_SIZE, DATA_MAX_SIZE, shm_obj)) {
                strcpy(err_msg, "[queue_init] should not fail with correct "
                                "input args");
                *queue_p = queue;
                destroy_pthread_stuff();
                return -1;
        }

        if (start(&consumer_1,
                  consumer_routine,
                  queue,
                  shm_obj,
                  err_msg,
                  queue_p,
                  "1st consumer") == -1) {
                destroy_pthread_stuff();
                return -1;
        }

        if (start(&supplier_1,
                  supplier_routine,
                  queue,
                  shm_obj,
                  err_msg,
                  queue_p,
                  "1st supplier") == -1) {
                destroy_pthread_stuff();
                return -1;
        }

        if (start(&consumer_2,
                  consumer_routine,
                  queue,
                  shm_obj,
                  err_msg,
                  queue_p,
                  "2nd consumer") == -1) {
                destroy_pthread_stuff();
                return -1;
        }

        if (start(&supplier_2,
                  supplier_routine,
                  queue,
                  shm_obj,
                  err_msg,
                  queue_p,
                  "2nd supplier") == -1) {
                destroy_pthread_stuff();
                return -1;
        }

        pthread_mutex_lock(start_mutex);
        *start_flag = 1;
        pthread_cond_broadcast(start_cond);
        pthread_mutex_unlock(start_mutex);


        pthread_mutex_lock(finish_mutex);
        struct timespec tm = {
                .tv_sec = time(NULL) + COND_WAIT_SECS_THREAD,
                .tv_nsec = 0,
        };
        int status;
        while (*threads_finished != 4) {
                status = pthread_cond_timedwait(finish_cond,
                                                finish_mutex,
                                                &tm);
                if (status == ETIMEDOUT) {
                        break;
                }
        }

        if (*threads_finished != 4 && shm_obj == NULL) {
                pthread_mutex_unlock(finish_mutex);

                strcpy(err_msg, "deadlock has happen (most probably):");

                char *retval = NULL;
                tm.tv_sec = time(NULL) + THREAD_JOIN_TIMEOUT;
                tm.tv_nsec = 0;

                pthread_cancel(consumer_1);
                if (pthread_timedjoin_np(consumer_1,
                                         (void *)&retval,
                                         &tm) == ETIMEDOUT) {
                        retval = "failed to cancel "
                                 "(most probably waiting on mutex)";
                }

                if (retval) {
                        if (retval == PTHREAD_CANCELED) {
                                retval = "cancelled";
                        }
                        sprintf(err_msg,
                                "%s [1st consumer: %s]",
                                err_msg,
                                retval);
                }
                retval = NULL;

                pthread_cancel(supplier_1);
                if (pthread_timedjoin_np(supplier_1,
                                         (void *)&retval,
                                         &tm) == ETIMEDOUT) {
                        retval = "failed to cancel "
                                 "(most probably waiting on mutex)";
                }

                if (retval) {
                        if (retval == PTHREAD_CANCELED) {
                                retval = "cancelled";
                        }
                        sprintf(err_msg,
                                "%s [1st supplier: %s]",
                                err_msg,
                                retval);
                }
                retval = NULL;

                pthread_cancel(consumer_2);
                if (pthread_timedjoin_np(consumer_2,
                                         (void *)&retval,
                                         &tm) == ETIMEDOUT) {
                        retval = "failed to cancel "
                                 "(most probably waiting on mutex)";
                }

                if (retval) {
                        if (retval == PTHREAD_CANCELED) {
                                retval = "cancelled";
                        }
                        sprintf(err_msg,
                                "%s [2nd consumer: %s]",
                                err_msg,
                                retval);
                }
                retval = NULL;

                pthread_cancel(supplier_2);
                if (pthread_timedjoin_np(supplier_2,
                                         (void *)&retval,
                                         &tm) == ETIMEDOUT) {
                        retval = "failed to cancel "
                                 "(most probably waiting on mutex)";
                }

                if (retval) {
                        if (retval == PTHREAD_CANCELED) {
                                retval = "cancelled";
                        }
                        sprintf(err_msg,
                                "%s [2nd supplier: %s]",
                                err_msg,
                                retval);
                }
                retval = NULL;

                *queue_p = queue;
                destroy_pthread_stuff();
                return -1;
        } else if (*threads_finished != 4 && shm_obj != NULL) {
                strcpy(err_msg, "deadlock has happen (most probably); "
                                "not all processes reported success");
                *queue_p = queue;
                destroy_pthread_stuff();
                return -1;
        }

        pthread_mutex_unlock(finish_mutex);

        if (shm_obj == NULL) {
                pthread_join(consumer_1, NULL);
                pthread_join(supplier_1, NULL);
                pthread_join(consumer_2, NULL);
                pthread_join(supplier_2, NULL);
        }

        queue_destroy(queue);
        destroy_pthread_stuff();

        return 0;
}

static int test_queue_dealock_private(char *err_msg, queue_t **queue_p) {
        return test_queue_deadlock(err_msg, queue_p, NULL);
}

static int test_queue_dealock_pshared(char *err_msg, queue_t **queue_p) {
        return test_queue_deadlock(err_msg,
                                   queue_p,
                                   SHM_OBJ);
}

int test_queue(char *err_msg) {
        queue_t *queue;

        if (test_queue_api(err_msg, &queue)) {
                goto err;
        }

        queue = NULL; /* we want a "fresh" queue in the next series of tests */

        if (test_queue_dealock_private(err_msg, &queue)) {
                goto err;
        }

        if (test_queue_dealock_pshared(err_msg, &queue)) {
                goto err;
        }

        return 0;

    err:
        /* do-while is needed because a label can only be part of a statement */
        do {
                FILE *stream;
                if (queue != NULL)  {
                        stream = fopen("./test-queue.dump", "w");
                        if (!stream) {
                                return -1;
                        }

                        queue_print(stream, queue);
                }
                queue_destroy(queue);

                /* does not really want to know if fclose fail here or not */
                fclose(stream);
        } while(0);

        return -1;
}
