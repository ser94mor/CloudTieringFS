#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <linux/limits.h>
#include <dotconf.h>

#include "cloudtiering.h"
#include "log.h"

static conf_t *conf = NULL;

static DOTCONF_CB(fs_mount_point);
static DOTCONF_CB(scanfs_iter_tm_sec);
static DOTCONF_CB(ev_start_rate);
static DOTCONF_CB(ev_stop_rate);
static DOTCONF_CB(ev_q_max_size);
static DOTCONF_CB(logger);

static const configoption_t options[] = {
        { "FsMountPoint",         ARG_STR,    fs_mount_point, NULL, CTX_ALL },
        { "ScanfsIterTimeoutSec", ARG_INT,    scanfs_iter_tm_sec,  NULL, CTX_ALL },
        { "EvictStartRate",       ARG_DOUBLE, ev_start_rate,  NULL, CTX_ALL },
        { "EvictStopRate",        ARG_DOUBLE, ev_stop_rate,   NULL, CTX_ALL },
        { "EvictQueueMaxSize",    ARG_INT,    ev_q_max_size,  NULL, CTX_ALL },
        { "LoggingFramework",     ARG_STR,    logger,         NULL, CTX_ALL },
        LAST_OPTION
};

static DOTCONF_CB(fs_mount_point) {
        strcpy(conf->fs_mount_point, cmd->data.str);
        return NULL;
}

static DOTCONF_CB(scanfs_iter_tm_sec) {
        conf->scanfs_iter_tm_sec = (time_t)cmd->data.value;
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

static DOTCONF_CB(logger) {
        if (strcmp(cmd->data.str, "syslog") == 0) {
                /* syslog case */
                conf->logger.open_log  = syslog_open_log;
                conf->logger.log       = syslog_log;
                conf->logger.close_log = syslog_close_log;

                conf->logger.error = SYSLOG_ERROR;
                conf->logger.info  = SYSLOG_INFO;
                conf->logger.debug = SYSLOG_DEBUG;
        } else {
                /* default case */
                conf->logger.open_log  = default_open_log;
                conf->logger.log       = default_log;
                conf->logger.close_log = default_close_log;

                conf->logger.error = DEFAULT_ERROR;
                conf->logger.info  = DEFAULT_INFO;
                conf->logger.debug = DEFAULT_DEBUG;
        }
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
 * @brief init_dependent_members Initialize remaining members of configuration.
 * @return 0 on success, -1 on failure
 */
static int init_dependent_members(void) {
        /* pathconf will not change errno and will return -1 if requested resource does not have limit */
        errno = 0;
        ssize_t path_max = pathconf(conf->fs_mount_point, _PC_PATH_MAX);
        if ((path_max == -1) && !errno) {
                path_max = PATH_MAX;
        } else if (path_max == -1) {
                return -1;
        }

        conf->path_max = path_max;

        return 0;
}

/**
 * @brief readconf Read configuration from file.
 * @note Does not handle the case when line length exceeded LINE_MAX limit.
 * @note Does not handle the case when file contains duplicate keys.
 * @param[in] conf_path Path to a configuration file. If NULL then use default configuration.
 * @return 0 on success, -1 on failure.
 */
int readconf(const char *conf_path) {
        if (conf != NULL) {
                return -1;
        }

        conf = (conf_t *)malloc(sizeof(conf_t));

        configfile_t *configfile;

        configfile = dotconf_create((char *)conf_path, options, NULL, NONE);
        if (!configfile) {
                return -1;
        }

        if (dotconf_command_loop(configfile) == 0) {
                return -1;
        }

        dotconf_cleanup(configfile);

        if (init_dependent_members()) {
                return -1;
        }

        return 0;
}
