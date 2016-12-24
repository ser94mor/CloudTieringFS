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
        queue_t *queue = (queue_t *)args;
        conf_t  *conf = getconf();

        int move_file_fails = 0;
        for (;;) {
                if (move_file(queue) == -1 && conf->move_file_max_fails != -1) {

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
        queue_t **in_out_q = (queue_t **)args;
        queue_t *in_queue  = in_out_q[0];
        queue_t *out_queue = in_out_q[1];
        conf_t *conf = getconf();

        int scanfs_fails = 0;
        for (;;) {
                if (scanfs(in_queue, out_queue) == -1 && conf->scanfs_max_fails != -1) {

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

static int init_data(queue_t **in_out_queue) {
        conf_t *conf = getconf();
        if (conf == NULL) {
                /* impossible because readconf() executed successfully */
                LOG(ERROR, "getconf() unexpectedly returned NULL; unable to start");
                return -1;
        }

        in_out_queue[0] = queue_alloc(conf->in_q_max_size, conf->path_max);
        if (in_out_queue[0] == NULL) {
                LOG(ERROR, "unable to allocate memory for in queue");
                return -1;
        }

        in_out_queue[1] = queue_alloc(conf->out_q_max_size, conf->path_max);
        if (in_out_queue[1] == NULL) {
                LOG(ERROR, "unable to allocate memory for out queue");
                return -1;
        }

        if (conf->ops.init_remote_store_access() == -1) {
                LOG(ERROR, "unable to establish connection to remote store");
                return -1;
        }

        return 0;
}

static int start_routines(queue_t **in_out_queue,
                          pthread_t *scanfs_thread,
                          pthread_t *move_file_in_thread,
                          pthread_t *move_file_out_thread) {
        int ret;

        ret = pthread_create(scanfs_thread, NULL, scanfs_routine, (void *)in_out_queue);
        if (ret != 0) {
                /* ret is errno in this case */
                LOG(ERROR, "pthread_create for scanfs_routine failed: %s", strerror(ret));
                return -1;
        }

        ret = pthread_create(move_file_in_thread, NULL, move_file_routine, (void *)in_out_queue[0]);
        if (ret != 0) {
                /* ret is errno in this case */
                LOG(ERROR, "pthread_create for move_file_routine (in queue) failed: %s", strerror(ret));
                return -1;
        }

        ret = pthread_create(move_file_out_thread, NULL, move_file_routine, (void *)in_out_queue[1]);
        if (ret != 0) {
                /* ret is errno in this case */
                LOG(ERROR, "pthread_create for move_file_routine (out queue) failed: %s", strerror(ret));
                return -1;
        }

        return 0;
}

int main(int argc, char *argv[]) {
        /* array containing in queue as a first element and out queue as a second element */
        queue_t *in_out_queue[2];

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
        if (readconf(argv[1])) {
                fprintf(stderr, "failed to read configuration file %s", argv[1]);
                return EXIT_FAILURE;
        }

        /* here configuration has successfully been read and logger structure was initialized */
        OPEN_LOG(argv[0]);

        /* initialize variables that will be used in whole program */
        if (init_data(in_out_queue) == -1) {
                return EXIT_FAILURE;
        }

        /* start all routines composing business logic of this program */
        if (start_routines(in_out_queue, &scanfs_thread, &move_file_in_thread, &move_file_out_thread)) {
                return EXIT_FAILURE;
        }

        monitor_threads(&scanfs_thread, &move_file_in_thread, &move_file_out_thread);

        return EXIT_FAILURE;
}
