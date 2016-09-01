#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>

#include "log.h"
#include "conf.h"

/**
 * @brief init_default_conf Initializes configuration with default values.
 */
static void init_default_conf() {
    conf.fs_root_dir = ORANGEFS_CLIENT_ROOT_DIRECTORY;
    conf.ev_session_tm = EVICT_SESSION_TIMEOUT;
    conf.start_ev_rate = START_EVICT_SPACE_RATE_LIMIT;
    conf.stop_ev_rate = STOP_EVICT_SPACE_RATE_LIMIT;
    conf.ev_q_max_size = EVICT_QUEUE_MAX_SIZE;
    conf.log_level = DEBUG;
    conf.tier_type[0] = ORANFEFS_TIER_TYPE;
    conf.tier_type[1] = S3_TIER_TYPE;


    conf.cold_tier_dir = TIER_COLD_HIDDEN_DIR;        /* temporary */
    conf.cold_tier_dir_mode = ORANGEFS_CLIENT_ROOT_DIRECTORY_MODE;  /* temporary */
    conf.evict_session_sleep = EVICT_SESSION_SLEEP; /* temporary */
}

static int init_conf_struct_member(const char *key, const char *val, const size_t val_len) {
    if (!strcmp("fs_root_dir", key)) {
        conf.fs_root_dir = (char *)malloc(sizeof(char) * (val_len + 1));
        strcpy(conf.fs_root_dir, val);
    } else if (!strcmp("ev_session_tm", key)) {
        conf.ev_session_tm = strto
    }
}

/**
 * @brief parse_conf_file Parses configuration file. Assigns found key-value pairs
 *                        to the appropriate records in conf_t configuration.
 * @param stream Opened FILE stream.
 * @return 0 on success, non-0 on failure.
 */
static int parse_conf_file(FILE *stream) {
    char buf[LINE_MAX]; /* buffer to hold line containing key-value pair */
    int key[2];         /* first element - begining offset; last - length */
    int val[2];

    while (!fgets(buf, LINE_MAX, stream)) {
        key[0] = key[1] = -1;
        val[0] = val[1] = -1;
        int i = 0;
        while (buf[i] != '\0') {
            if (isgraph(buf[i])) {
                if (key[0] == -1) {
                    key[0] = i;
                    if (!isgraph(buf[i + 1])) {
                        key[1] = 1;
                    }
                } else if (key[1] == -1 && !isgraph(buf[i + 1])) {
                    key[1] = i - key[0];
                } else if (val[0] == -1) {
                    val[0] = i;
                    if (!isgraph(buf[i + 1])) {
                        val[1] = 1;
                        break;
                    }
                } else if (val[1] == -1 && !isgraph(buf[i + 1])) {
                    val[1] = i - val[0];
                    break;
                }
            }

            ++i;
            continue;
        }

        /* ensure that the string was correct */
        if (key[0] == -1) {
            /* empty line */
            continue;
        }

        char key_str[key[1] + 1];
        strncpy(key_str, buf + key[0], key[1]);
        key_str[key[1]] = '\0';

        if (val[0] == -1) {
            /* no value specified */
            LOG(ERROR, "no value specified for configuration parameter '%s'", key_str);
            return EXIT_FAILURE;
        }

        char val_str[val[1] + 1];
        strncpy(val_str, buf + val[0], val[1]);
        val_str[val[1]] = '\0';

        if (init_conf_struct_member(key_str, val_str, val[1])) {
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

int parse_conf_file(FILE * stream) {
    char buf[LINE_MAX]; /* buffer to hold line containing key-value pair */

    while (!fgets(buf, LINE_MAX, stream)) {
        const char *delim = " \t\n";

        char *key = strtok(buf, delim);
        char *val = strok(NULL, delim);

        if (init_conf_struct_member(key_str, val_str, val[1])) {
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

/**
 * @brief readconf Read configuration from file.
 * @note Does not handle the case when line length exceeded LINE_MAX limit.
 * @note Does not handle the case when file contains duplicate keys.
 * @param[in] conf_path Path to a configuration file. If NULL then use default configuration.
 * @return 0 on success, non-0 on failure.
 */
int readconf(const char *conf_path) {
    /* initialize configuration with default values to avoid non-initialized conf_t members */
    init_default_conf();

    if (conf_path == NULL) {
        return EXIT_SUCCESS; /* NULL indicates default configuration */
    }

    FILE *stream;

    stream = fopen(conf_path, "r");
    if (!stream) {
        LOG(ERROR, "%s", strerror(errno));
        return EXIT_FAILURE;
    }

    int ret = parse_conf_file(stream);

    if (fclose(stream)) {
        /* failure of fclose often indicates some serious error */
        LOG(ERROR, "%s", strerror(errno));
        return EXIT_FAILURE;
    }

    if (ret) {
        /* parsing error */
        return EXIT_FAILURE;
    }
}
