#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "cloudtiering.h"

static const char *test_conf_str = "FsMountPoint             /foo/bar\n"\
                                   "ScanfsIterTimeoutSec     100\n"\
                                   "EvictStartRate           0.8\n"\
                                   "EvictStopRate            0.7\n"\
                                   "EvictQueueMaxSize        9999\n"\
                                   "LoggingFramework         default\n";

static int create_test_conf_file() {
        /* create configuration file */
        FILE *stream;
        
        int ret = mkdir("./validate", S_IRWXU | S_IRWXG | S_IRWXO);
        if (ret == -1 && errno != EEXIST) {
                return -1;
        }

        stream = fopen("./validate/test.conf", "w");
        if (!stream) {
                return -1;
        }

        size_t len_items = strlen(test_conf_str) + sizeof(char);
        if (fwrite(test_conf_str, sizeof(char), len_items, stream) != len_items) {
                return -1;
        }

        if (fclose(stream)) {
                return -1;
        }

        return 0;
}

int test_conf(char *err_msg) {
        if (create_test_conf_file() == -1) {
                strcpy(err_msg, "unable to create configuration file ./validate/test.conf");
                return -1;
        }

        conf_t *conf = getconf(); /* should be NULL before readconf call */
        if (conf != NULL) {
                strcpy(err_msg, "'getconf' should return NULL before 'readconf' was ever invoked");
                return -1;
        }

        /* validate readconf(...) result */
        if (readconf("./validate/test.conf")) {
                strcpy(err_msg, "'readconf' function failed");
                return -1;
        }

        conf = getconf();
        if (strcmp(conf->fs_mount_point, "/foo/bar") ||
            conf->scanfs_iter_tm_sec != 100 ||
            conf->ev_start_rate != 0.8 ||
            conf->ev_stop_rate != 0.7 ||
            conf->ev_q_max_size != 9999 ||
            strncmp(conf->logger.name, "default", sizeof(conf->logger.name))
        ) {
                strcpy(err_msg, "configuratition contains incorrect values");
                return -1;
        }

        return 0;
}

