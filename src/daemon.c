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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <pthread.h>

#include "cloudtiering.h"

static void monitor_threads(pthread_t *scanfs_thread,
                            pthread_t *move_file_in_thread,
                            pthread_t *move_file_out_thread) {
        /* TODO: implement something smart */
        for(;;){}
}


static void *move_file_routine(void *args) {
        struct queue_tuple *qtuple = (struct queue_tuple *)args;
        conf_t  *conf = get_conf();

        int move_file_fails = 0;
        for (;;) {
                if (move_file(qtuple) == -1 && conf->move_file_max_fails != -1) {

                        /* handle failure of move_file in case conf->move_file_fails has limit (!= -1) */
                        ++move_file_fails;
                        if (move_file_fails > conf->move_file_max_fails) {
                                LOG(ERROR, "scanfs limit on failures exceeded (limit is %d)", conf->move_file_max_fails);
                                return NULL;
                        }
                        LOG(ERROR, "move_file execution failed (%d/%d)", move_file_fails, conf->move_file_max_fails);
                        continue;
                }
        }

        return NULL; /* unreachable place */
}

static void *scanfs_routine(void *args) {
        struct queue_tuple *queue_tuple = (struct queue_tuple *)args;
        conf_t *conf = get_conf();

        int scanfs_fails = 0;
        for (;;) {
                if (scanfs(queue_tuple->download_queue,
                           queue_tuple->upload_queue) == -1 &&
                    conf->scanfs_max_fails != -1) {

                        /* handle failure of scanfs in case conf->scanfs_max_fails has limit (!= -1) */
                        ++scanfs_fails;
                        if (scanfs_fails > conf->scanfs_max_fails) {
                                LOG(ERROR, "scanfs limit on failures exceeded (limit is %d)", conf->scanfs_max_fails);
                                return NULL;
                        }
                        LOG(ERROR, "scanfs execution failed (%d/%d)", scanfs_fails, conf->scanfs_max_fails);

                }
        }

        return NULL; /* unreachable place */
}

static int init_data(struct queue_tuple *qtuple) {
        conf_t *conf = get_conf();
        if (conf == NULL) {
                /* impossible because readconf() executed successfully */
                LOG(ERROR, "get_conf() unexpectedly returned NULL; unable to start");
                return -1;
        }

        qtuple->upload_queue = QUEUE_ALLOC(qtuple->upload_queue,
                                           conf->in_q_max_size,
                                           conf->path_max);

        if (qtuple->upload_queue == NULL) {
                LOG(ERROR, "unable to allocate memory for upload queue");
                return -1;
        }

        qtuple->download_queue = QUEUE_ALLOC(qtuple->download_queue,
                                             conf->in_q_max_size,
                                             conf->path_max);
        if (qtuple->download_queue == NULL) {
                LOG(ERROR, "unable to allocate memory for download queue");
                return -1;
        }

        if (get_ops()->connect() == -1) {
                LOG(ERROR, "unable to establish connection to remote store");
                return -1;
        }

        return 0;
}

static int start_routines(struct queue_tuple *qtuple,
                          pthread_t *scanfs_thread,
                          pthread_t *move_file_in_thread,
                          pthread_t *move_file_out_thread) {
        int ret;

        ret = pthread_create(scanfs_thread,
                             NULL,
                             scanfs_routine,
                             qtuple);
        if (ret != 0) {
                /* ret is errno in this case */
                LOG(ERROR,
                    "pthread_create for scanfs_routine failed: %s",
                    strerror(ret));
                return -1;
        }

        ret = pthread_create(move_file_in_thread,
                             NULL,
                             move_file_routine,
                             qtuple);
        if (ret != 0) {
                /* ret is errno in this case */
                LOG(ERROR,
                    "pthread_create for move_file_routine (download queue) "
                    "failed: %s",
                    strerror(ret));
                return -1;
        }

        ret = pthread_create(move_file_out_thread,
                             NULL,
                             move_file_routine,
                             qtuple);
        if (ret != 0) {
                /* ret is errno in this case */
                LOG(ERROR,
                    "pthread_create for move_file_routine (upload queue) "
                    "failed: %s",
                    strerror(ret));
                return -1;
        }

        return 0;
}

int main(int argc, char *argv[]) {
        struct queue_tuple qtuple;

        /* thread that should scan file system and add elements to in and out queues */
        pthread_t scanfs_thread;

        /* thread that should move files from remote store to local */
        pthread_t move_file_in_thread;

        /* thread that should move files from local store to remote */
        pthread_t move_file_out_thread;

        /* validate number of imput arguments */
        if (argc != 2) {
                fprintf(stderr, "1 argument was expected but %d provided", argc - 1);
                return EXIT_FAILURE;
        }

        /* read configuration file specified via input argument */
        if (read_conf(argv[1])) {
                fprintf(stderr, "failed to read configuration file %s", argv[1]);
                return EXIT_FAILURE;
        }

        /* here configuration has successfully been read and logger structure was initialized */
        OPEN_LOG(argv[0]);

        /* initialize variables that will be used in whole program */
        if (init_data(&qtuple) == -1) {
                return EXIT_FAILURE;
        }

        /* start all routines composing business logic of this program */
        if (start_routines(&qtuple,
                           &scanfs_thread,
                           &move_file_in_thread,
                           &move_file_out_thread)) {
                return EXIT_FAILURE;
        }

        monitor_threads(&scanfs_thread, &move_file_in_thread, &move_file_out_thread);

        return EXIT_FAILURE;
}