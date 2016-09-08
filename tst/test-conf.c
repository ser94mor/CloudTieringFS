#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "conf.h"

static const char *test_conf_str = "FsMountPoint             /foo/bar\n"\
                                   "EvictSessionTimeout      100\n"\
                                   "EvictStartRate           0.8\n"\
                                   "EvictStopRate            0.7\n"\
                                   "EvictQueueMaxSize        9999\n";

static int create_test_conf_file() {
        /* create configuration file in the current directory */
        FILE *stream;

        stream = fopen("./test_readconf.conf", "w");
        if (!stream) {
                return 1;
        }

        size_t len_items = strlen(test_conf_str) + sizeof(char);
        if (fwrite(test_conf_str, sizeof(char), len_items, stream) != len_items) {
                return 1;
        }

        if (fclose(stream)) {
                return 1;
        }

        return 0;
}

int test_readconf(char *err_msg) {
        if (create_test_conf_file()) {
                strcpy(err_msg, "unable to create test configuration file");
                return 1;
        }

        conf_t *conf = getconf(); /* should be NULL before readconf call */
        if (conf != NULL) {
                strcpy(err_msg, "'getconf' should return NULL before 'readconf' was ever invoked");
                return 1;
        }

        /* validate readconf(...) result */
        if (readconf("./test_readconf.conf")) {
                strcpy(err_msg, "'readconf' function failed");
                return 1;
        }

        conf = getconf();
        if (strcmp(conf->fs_mount_point, "/foo/bar") ||
            conf->ev_session_tm != 100 ||
            conf->ev_start_rate != 0.8 ||
            conf->ev_stop_rate != 0.7 ||
            conf->ev_q_max_size != 9999) {
                strcpy(err_msg, "configuratition contains incorrect values");
                return 1;
        }

        /* remove configuration file which has been generated earlier */
        if (unlink("./test_readconf.conf")) {
                strcpy(err_msg, "unable to remove test configuration file");
                return 1;
        }

        return 0;
}

