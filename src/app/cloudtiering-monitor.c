#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#include "cloudtiering.h"

const queue_t *evict_queue = NULL;

static int start_scanfs_thread(void) {
        if (scanfs(evict_queue) == -1) {
                return -1;
        }
        return 0;
}

static int init_data(void) {
        conf_t *conf = getconf();
        evict_queue = queue_alloc(conf->ev_q_max_size, conf->path_max);

        return 0;
}

int main(int argc, char *argv[]) {
        OPEN_LOG(argv[0]);

        if (argc != 2) {
                LOG(ERROR, "1 argument was expected but %d provided", argc - 1);
                return EXIT_FAILURE;
        }

        if (readconf(argv[1])) {
                LOG(ERROR, "failed to read configuration file %s", argv[1]);
                return EXIT_FAILURE;
        }

        if (init_data()) {
                LOG(ERROR, "failed to init data structures required for work");
                return EXIT_FAILURE;
        }
        
        start_scanfs_thread();
        
        //start_filessystem_info_updater_thread();
        //start_eviction_queue_updater_thread();
        //start_data_evictor_thread();

        return EXIT_SUCCESS;
}
