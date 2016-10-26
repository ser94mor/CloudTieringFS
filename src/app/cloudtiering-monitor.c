#include <stdlib.h>
#include <unistd.h>

#include "cloudtiering.h"

queue_t *evict_queue = NULL;

int init_data(void) {
        conf_t *conf = getconf();
        evict_queue = queue_alloc(conf->ev_q_max_size, pathconf(conf->fs_mount_point, _PC_NAME_MAX));
        
        return 0;
}

int main(int argc, char *argv[]) {
        OPEN_LOG();
        
        if (argc != 2) {
                LOG(ERROR, "1 argument was expected but %d provided", argc - 1);
                return EXIT_FAILURE;
        }
        
        if (readconf(argv[1])) {
                LOG(ERROR, "error reading configuration file %s", argv[1])
                return EXIT_FAILURE;
        }
        
        
        init_data();
        

        //initialize_datastuctures();
        //start_filessystem_info_updater_thread();
        //start_eviction_queue_updater_thread();
        //start_data_evictor_thread();

        return EXIT_SUCCESS;
}
