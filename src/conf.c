#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <time.h>
#include <dotconf.h>

#include "conf.h"

static conf_t *conf = NULL;

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

static DOTCONF_CB(fs_mount_point) {
        strcpy(conf->fs_mount_point, cmd->data.str);
        return NULL;
}

static DOTCONF_CB(ev_session_tm) {
        conf->ev_session_tm = (time_t)cmd->data.value;
        return NULL;
}

static DOTCONF_CB(ev_start_rate) {
        conf->ev_start_rate = (double)cmd->data.dvalue;
        return NULL;
}

static DOTCONF_CB(ev_stop_rate) {
        conf->ev_stop_rate = (double)cmd->data.dvalue;
        return NULL;
}

static DOTCONF_CB(ev_q_max_size) {
        conf->ev_q_max_size = (size_t)cmd->data.value;
        return NULL;
}

/**
 * @brief getconf Get pointer to configuration.
 * @return Pointer to conf_t or NULL if readconf has not been invoked yet.
 */
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
        conf = (conf_t *)malloc(sizeof(conf_t));

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
