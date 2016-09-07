#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

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

        size_t len_items = strlen_s(test_conf_str) + sizeof(char);
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
                strcpy_s(err_msg, LINE_MAX, "unable to create test configuration file");
                return 1;
        }

        /* validate readconf(...) result */
        if (readconf("./test_readconf.conf")) {
                strcpy_s(err_msg, LINE_MAX, "'readconf' function failed");
                return 1;
        }

        conf_t *conf = getconf();
        /* TODO: finish test case */

        return 0;
}

