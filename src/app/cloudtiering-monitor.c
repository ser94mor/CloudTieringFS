#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "cloudtiering.h"

static void *scanfs_routine(void *args) {
        queue_t **in_out_q = (queue_t **)args;
        queue_t *in_queue  = in_out_q[0];
        queue_t *out_queue = in_out_q[1];
        conf_t *conf = getconf();

        time_t beg_time;  /* start of scanfs execution */
        time_t end_time;  /* finish of scanfs execution */
        time_t diff_time; /* difference between beg_time and end_time */
        int scanfs_fails = 0;
        for (;;) {
                beg_time = time(NULL);
                if (scanfs(in_queue, out_queue) == -1 && conf->scanfs_max_fails != -1) {

                        /* handle failure of scanfs in case conf->scanfs_max_fails has limit (!= -1) */
                        ++scanfs_fails;
                        if (scanfs_fails > conf->scanfs_max_fails) {
                                LOG(ERROR, "scanfs limit on failures exceeded (limit is %d)", conf->scanfs_max_fails);
                                return NULL;
                        }
                        LOG(ERROR, "scanfs execution failed (%d/%d)", scanfs_fails, conf->scanfs_max_fails);

                }
                end_time = time(NULL);
                diff_time = (time_t)difftime(end_time, beg_time);

                if (diff_time < conf->scanfs_iter_tm_sec) {
                        sleep((unsigned int)(conf->scanfs_iter_tm_sec - diff_time));
                }
        }

        return NULL; /* unreachable place */
}


int main(int argc, char *argv[]) {
        conf_t *conf = NULL;       /* pointer to program configuration */
        queue_t *in_queue = NULL;  /* pointer to queue that stores files to be moved to local store */
        queue_t *out_queue = NULL; /* pointer to queue that stores files to be moved to remote store */

        if (argc != 2) {
                fprintf(stderr, "1 argument was expected but %d provided", argc - 1);
                return EXIT_FAILURE;
        }

        if (readconf(argv[1])) {
                fprintf(stderr, "failed to read configuration file %s", argv[1]);
                return EXIT_FAILURE;
        }

        /* here configuration has successfully been read and logger structure was initialized */
        OPEN_LOG(argv[0]);

        conf = getconf();
        if (conf == NULL) {
                /* impossible because readconf() executed successfully */
                LOG(ERROR, "getconf() unexpectedly returned NULL; unable to start");
                return EXIT_FAILURE;
        }

        in_queue = queue_alloc(conf->in_q_max_size, conf->path_max);
        if (in_queue == NULL) {
                LOG(ERROR, "unable to allocate memory for in_queue");
                return EXIT_FAILURE;
        }

        out_queue = queue_alloc(conf->out_q_max_size, conf->path_max);
        if (out_queue == NULL) {
                LOG(ERROR, "unable to allocate memory for out_queue");
                return EXIT_FAILURE;
        }

        pthread_t scanfs_thread;
        int ret;

        queue_t *in_out_queue[] = {in_queue, out_queue};
        ret = pthread_create(&scanfs_thread, NULL, scanfs_routine, (void *)in_out_queue);
        if (ret != 0) {
                /* ret is errno in this case */
                LOG(ERROR, "pthread_create for scanfs_routine failed: %s", strerror(ret));
                return EXIT_FAILURE;
        }
        //start_filessystem_info_updater_thread();
        //start_eviction_queue_updater_thread();
        //start_data_evictor_thread();

        for(;;){}

        /* if we reach this place then there was a failure in some thread */
        free(conf);
        queue_free(out_queue);
        queue_free(in_queue);

        return EXIT_FAILURE;
}
