#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ftw.h>
#include <syslog.h>
#include <dotconf.h>

#include "conf.h"

int main(int argc, char *argv[]) {
        /* open connection to syslog */
        openlog("tiering:fs-monitor", LOG_PID, LOG_USER);
        syslog(LOG_INFO, "starting file system monitor");

        if (argc != 2) {
                syslog(LOG_ERR, "incorrect number of arguments provided: %d; should be 1", argc);
                exit(EXIT_FAILURE);
        }

        /* reading configuration file provided as an argument */
        configfile_t *configfile = dotconf_create(argv[1], options, NULL, CASE_INSENSITIVE);
        if (!configfile) {
                syslog(LOG_ERR, "unable to proceed further due to configuration read failure");
                exit(EXIT_FAILURE);
        }

        if (dotconf_command_loop(configfile) == 0) {
                syslog(LOG_ERR, "error while reading configuration file");
                exit(EXIT_FAILURE);
        }

        dotconf_cleanup(configfile);

        initialize_datastuctures();
        start_filessystem_info_updater_thread();
        start_eviction_queue_updater_thread();
        start_data_evictor_thread();

        LOG(INFO, "file system monitor successfully started");

        exit(EXIT_SUCCESS);
}
