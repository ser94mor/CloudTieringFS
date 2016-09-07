#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <dotconf.h>

#include "conf.h"

static conf_t *conf = NULL;

typedef struct {
    char   fs_mount_point[PATH_MAX];      /* filesystem's root directory */
    time_t ev_session_tm;                 /* the lowest time interval between evict sessions */
    double ev_start_rate;                 /* start evicting files when storage is (start_ev_rate * 100)% full */
    double ev_stop_rate;                  /* stop evicting files when storage is (stop_ev_rate * 100)% full */
    size_t ev_q_max_size;                 /* maximum size of evict queue */
} conf_t;

FsMountPoint           /mnt/orangefs
EvictSessionTimeout    60
EvictStartRate         0.7
EvictStopRate          0.6
EvictQueueMaxSize      128

static DOTCONF_CB(fs_mount_point);
static DOTCONF_CB(ev_session_tm);
static DOTCONF_CB(ev_start_rate);
static DOTCONF_CB(ev_stop_rate);
static DOTCONF_CB(ev_q_max_size);

static const configoption_t options[] = {
        { "FsMountPoint",        ARG_STR,    fs_mount_point, NULL, CTX_ALL },
        { "EvictSessionTimeout", ARG_INT,    ev_session_tm,  NULL, CTX_ALL },
        { "EvictStartRate",      ARG_DOUBLE, ev_start_rate,  NULL, CTX_ALL },
        { "EvictStopRate",       ARG_DOUBLE, ev_stop_rate,   NULL, CTX_ALL },
        { "EvictQueueMaxSize",   ARG_INT,    ev_q_max_size,  NULL, CTX_ALL },
        LAST_OPTION
};

inline conf_t *getconf() {
        return conf;
}



/**
 * @brief readconf Read configuration from file.
 * @note Does not handle the case when line length exceeded LINE_MAX limit.
 * @note Does not handle the case when file contains duplicate keys.
 * @param[in] conf_path Path to a configuration file. If NULL then use default configuration.
 * @return 0 on success, non-0 on failure.
 */
int readconf(const char *conf_path) {
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
