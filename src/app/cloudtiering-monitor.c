#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>

#include "cloudtiering.h"

const queue_t *evict_queue = NULL;

static void *scanfs_routine(void *args) {
        return 0;
}


int main(int argc, char *argv[]) {
        conf_t *conf = NULL;         /* pointer to program configuration */
        queue_t *evict_queue = NULL; /* pointer to queue that stores candidates for eviction */
        
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
        
        evict_queue = queue_alloc(conf->ev_q_max_size, conf->path_max);
        if (evict_queue == NULL) {
                LOG(ERROR, "unable to allocate memory for evict_queue");
                return EXIT_FAILURE;
        }

        pthread_t scanfs_thread;
        int ret;

        ret = pthread_create(&scanfs_thread, NULL, scanfs_routine, (void *)evict_queue);
        if (!ret) {
                errno = ret;
                LOG(ERROR, "pthread_create for scanfs_routine failed: %s", strerror(errno));
                return EXIT_FAILURE;
        }
        //start_filessystem_info_updater_thread();
        //start_eviction_queue_updater_thread();
        //start_data_evictor_thread();

        return EXIT_SUCCESS;
}
