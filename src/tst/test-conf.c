#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "cloudtiering.h"

static const char *test_conf_str = "<General>\n"                             \
                                   "    FsMountPoint             /foo/bar\n" \
                                   "    LoggingFramework         default\n"  \
                                   "    RemoteStoreProtocol      s3\n"       \
                                   "</General>\n"                            \
                                   "<Internal>\n"                            \
                                   "    ScanfsIterTimeoutSec     100\n"      \
                                   "    ScanfsMaximumFailures    1\n"        \
                                   "    MoveFileMaximumFailures  2\n"        \
                                   "    MoveOutStartRate         0.8\n"      \
                                   "    MoveOutStopRate          0.7\n"      \
                                   "    OutQueueMaxSize          9999\n"     \
                                   "    InQueueMaxSize           1111\n"     \
                                   "</Internal>\n"                           \
                                   "<S3RemoteStore>\n"                       \
                                   "    S3DefaultHostname        s3_hostname\n"        \
                                   "    S3Bucket                 s3.bucket\n"          \
                                   "    S3AccessKeyId            test_access_key_id\n" \
                                   "    S3SecretAccessKey        test_secret_key\n"    \
                                   "</S3RemoteStore>\n";

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
            strcmp(conf->s3_default_hostname, "s3_hostname") ||
            strcmp(conf->s3_bucket, "s3.bucket") ||
            strcmp(conf->s3_access_key_id, "test_access_key_id") ||
            strcmp(conf->s3_secret_access_key, "test_secret_key") ||
            strcmp(conf->remote_store_protocol, "s3") ||
            conf->scanfs_iter_tm_sec != 100 ||
            conf->scanfs_max_fails != 1 ||
            conf->move_file_max_fails != 2 ||
            conf->move_out_start_rate != 0.8 ||
            conf->move_out_stop_rate != 0.7 ||
            conf->out_q_max_size != 9999 ||
            conf->in_q_max_size != 1111 ||
            strncmp(conf->logger.name, "default", sizeof(conf->logger.name))
        ) {
                strcpy(err_msg, "configuratition contains incorrect values");
                return -1;
        }

        return 0;
}

