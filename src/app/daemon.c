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

typedef struct {
        void *first;
        void *second;
} pair_t;

static void monitor_threads(pthread_t *scan_fs_thread,
                                      pthread_t *dowload_file_thread,
                                      pthread_t *upload_file_thread) {
        /* TODO: implement thread monitoring */
        for(;;){}
}

static void *transfer_files_loop(pair_t *pair,
                                 int (*action)(const char *),
                                 const char *action_name) {
        const size_t path_max_size = get_conf()->path_max;
        char path[path_max_size];
        unsigned long long failure_counter = 0;

        queue_t *primary_queue   = pair->first;
        queue_t *secondary_queue = pair->second;

        size_t path_size;
        int pop_res;
        for (;;) {
                pop_res = -1;
                path_size = path_max_size;

                if (primary_queue != NULL) {
                    pop_res = queue_try_pop(primary_queue,
                                            path,
                                            &path_size);
                }

                if (pop_res == -1 && secondary_queue != NULL) {
                        pop_res = queue_pop(secondary_queue,
                                            path,
                                            &path_size);
                }

                if (pop_res == -1) {
                        /* if the program's logic is correct these statements
                           will never be executed */

                        LOG(ERROR,
                            "corrupted queue_tuple_t structure "
                            "[reason: at least one member should be assigned]");

                        exit(EXIT_FAILURE);
                }

                if (action(path) == -1) {
                        /* continue execution even on failure */

                        if ((++failure_counter % 1024) == 0) {
                                /* periodically report about failures */

                                LOG(DEBUG,
                                    "failures of %s [counter: %llu]",
                                    action_name,
                                    failure_counter);
                        }
                }

                pthread_testcancel();
        }

        return "unreachable place";
}

static void *download_file_routine(void *args) {
        return transfer_files_loop((pair_t *)args,
                                   download_file,
                                   "download file");
}

static void *upload_file_routine(void *args) {
        return transfer_files_loop((pair_t *)args,
                                   upload_file,
                                   "upload file");
}

static void *scan_fs_routine(void *args) {
        pair_t *dow_upl_pair = args;
        pair_t *dow_queue_pair = dow_upl_pair->first;
        pair_t *upl_queue_pair = dow_upl_pair->second;

        queue_t *download_queue = dow_queue_pair->second;
        queue_t *upload_queue   = upl_queue_pair->second;

        unsigned long long failure_counter = 0;
        for (;;) {
                if (scan_fs(download_queue, upload_queue) == -1) {
                        /* continue execution even on failure */

                        if ((++failure_counter % 1024) == 0) {
                                /* periodically report about failures */

                                LOG(DEBUG,
                                    "failures of %s [counter: %llu]",
                                    "scan_fs",
                                    failure_counter);
                        }
                }
        }

        return "unreachable place";
}

static int init_data(pair_t *dow_queue_pair,
                     pair_t *upl_queue_pair) {
        conf_t *conf = get_conf();
        if (conf == NULL) {
                LOG(ERROR,
                    "get_conf() unexpectedly returned NULL; unable to start");
                return -1;
        }

        if (queue_init((queue_t **)&(dow_queue_pair->first),
                       conf->primary_download_queue_max_size,
                       conf->path_max,
                       QUEUE_SHM_OBJ) == -1) {
                LOG(ERROR,
                    "unable to allocate memory for primary download queue");
                return -1;
        }

        if (queue_init((queue_t **)&(dow_queue_pair->second),
                       conf->secondary_download_queue_max_size,
                       conf->path_max,
                       NULL) == -1) {
                LOG(ERROR,
                    "unable to allocate memory for secondary download queue");

                /* cleanup already allocated queues */
                queue_destroy(dow_queue_pair->first);

                return -1;
        }

        /* today there are no situations where hierarchy of queues
           established for upload action */
        upl_queue_pair->first = NULL;

        if (queue_init((queue_t **)&(upl_queue_pair->second),
                       conf->secondary_upload_queue_max_size,
                       conf->path_max,
                       NULL) == -1) {
                LOG(ERROR,
                    "unable to allocate memory for secondary upload queue");

                /* cleanup already allocated queues */
                queue_destroy(dow_queue_pair->first);
                queue_destroy(dow_queue_pair->second);
                queue_destroy(upl_queue_pair->first);

                return -1;
        }

        if (get_ops()->connect() == -1) {
                LOG(ERROR, "unable to establish connection to remote storage");

                queue_destroy(dow_queue_pair->first);
                queue_destroy(dow_queue_pair->second);
                queue_destroy(upl_queue_pair->first);
                queue_destroy(upl_queue_pair->second);

                return -1;
        }

        return 0;
}

static int start_routines(pair_t    *dow_queue_pair,
                          pair_t    *upl_queue_pair,
                          pthread_t *scan_fs_thread,
                          pthread_t *download_file_thread,
                          pthread_t *upload_file_thread) {
        int ret;

        pair_t dow_upl_pair = {
                .first = dow_queue_pair,
                .second = upl_queue_pair,
        };
        ret = pthread_create(scan_fs_thread,
                             NULL,
                             scan_fs_routine,
                             &dow_upl_pair);
        if (ret != 0) {
                /* ret is errno in this case */
                LOG(ERROR,
                    "pthread_create for scan_fs_routine failed "
                    "[reason: %s]",
                    strerror(ret));
                return -1;
        }

        ret = pthread_create(download_file_thread,
                             NULL,
                             download_file_routine,
                             dow_queue_pair);
        if (ret != 0) {
                /* ret is errno in this case */
                LOG(ERROR,
                    "pthread_create for download_file_routine failed "
                    "[reason: %s]",
                    strerror(ret));
                return -1;
        }

        ret = pthread_create(upload_file_thread,
                             NULL,
                             upload_file_routine,
                             upl_queue_pair);
        if (ret != 0) {
                /* ret is errno in this case */
                LOG(ERROR,
                    "pthread_create for upload_file_routine failed "
                    "[reason: %s]",
                    strerror(ret));
                return -1;
        }

        return 0;
}

int main(int argc, char *argv[]) {
        pair_t dow_queue_pair;
        pair_t upl_queue_pair;

        /* thread that should scan file system and add elements to download
           and upload queues */
        pthread_t scan_fs_thread;

        /* thread that should move files from remote storage to local */
        pthread_t download_file_thread;

        /* thread that should move files from local storage to remote */
        pthread_t upload_file_thread;

        /* validate number of input arguments */
        if (argc != 2) {
                fprintf(stderr,
                        "1 argument was expected but %d provided",
                        argc - 1);
                return EXIT_FAILURE;
        }

        /* read configuration file specified via input argument */
        if (read_conf(argv[1])) {
                fprintf(stderr,
                        "failed to read configuration file %s",
                        argv[1]);
                return EXIT_FAILURE;
        }

        /* here configuration has successfully been read and logger
           structure has been initialized */
        OPEN_LOG(argv[0]);

        /* initialize variables that will be used in whole program */
        if (init_data(&dow_queue_pair, &upl_queue_pair) == -1) {
                return EXIT_FAILURE;
        }

        /* start all routines composing business logic of this program */
        if (start_routines(&dow_queue_pair,
                           &upl_queue_pair,
                           &scan_fs_thread,
                           &download_file_thread,
                           &upload_file_thread) == -1) {
                return EXIT_FAILURE;
        }

        monitor_threads(&scan_fs_thread,
                        &download_file_thread,
                        &upload_file_thread);

        /* this place is unreachable */
        return EXIT_FAILURE;
}
