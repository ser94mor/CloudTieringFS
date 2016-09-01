#include <stdlib.h>

#include "log.h"
#include "conf.h"



int main(int argc, char *argv[]) {
    LOG(INFO, "starting data-ejector");

    if (argc != 2) {
        LOG(FATAL, "incorrect number of arguments provided: %d; should be 1", argc);
        exit(EXIT_FAILURE);
    }

    if(readconf(argv[1])) {
        LOG(FATAL, "unable to proceed further due to configuration read failure");
        exit(EXIT_FAILURE);
    }

    read_configuration();
    initialize_datastuctures();
    start_filessystem_info_updater_thread();
    start_eviction_queue_updater_thread();
    start_data_evictor_thread();

    LOG(INFO, "data-ejector successfully started");

    return EXIT_SUCCESS;
}
