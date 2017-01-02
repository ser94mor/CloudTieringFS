/**
 * Copyright (C) 2016, 2017  Sergey Morozov <sergey94morozov@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
static log_t  *log  = NULL;
static ops_t  *ops  = NULL;


/*******************************************************************************
* Logging frameworks' definitions.                                             *
*******************************************************************************/

/* declaration of structures for each type of supported logging framework */
LOGGERS(DECLARE_LOGGER, SEMICOLON);

/* string names of logging frameworks */
static const char *log_str[] = {
        LOGGERS(STRINGIFY, COMMA),
};

/* array of pointers to logging frameworks' structures */
static log_t *log_arr[] = {
        LOGGERS(LOGGER_ADDR, COMMA),
};

/* number of supported logging frameworks */
static const size_t log_count = LOGGERS(MAP_TO_ONE, PLUS);


/*******************************************************************************
* Sections' definitions.                                                       *
*******************************************************************************/

/* declarations of each section's begining and end token strings */
SECTIONS(DECLARE_SECTION_STR, SEMICOLON);

/* a macro-function that declares dotconf's begining and end section
   callback functions */
#define DECLARE_SECTION_CALLBACKS(elem)              \
        static DOTCONF_CB(beg_##elem##_section_cb) { \
                return NULL;                         \
        }                                            \
        static DOTCONF_CB(end_##elem##_section_cb) { \
                return NULL;                         \
        }


/*******************************************************************************
* Operations' definitions.                                                     *
*******************************************************************************/

/* string names of supported object storage protocols */
static const char *protocol_str[] = {
        PROTOCOLS(STRINGIFY, COMMA),
};

/* declaration of ops_t data-structure for each kind of protocol */
PROTOCOLS(DECLARE_OPS, SEMICOLON);


/*******************************************************************************
* Dotconf callback functions' definitions.                                     *
*******************************************************************************/

SECTIONS(DECLARE_SECTION_CALLBACKS, EMPTY)

static DOTCONF_CB(fs_mount_point_cb) {
        strcpy(conf->fs_mount_point, cmd->data.str);
        return NULL;
}

static DOTCONF_CB(scanfs_iter_tm_sec_cb) {
        conf->scanfs_iter_tm_sec = (time_t)cmd->data.value;
        return NULL;
}

static DOTCONF_CB(scanfs_max_fails_cb) {
        conf->scanfs_max_fails = (int)cmd->data.value;
        return NULL;
}

static DOTCONF_CB(move_file_max_fails_cb) {
        conf->move_file_max_fails = (int)cmd->data.value;
        return NULL;
}

static DOTCONF_CB(move_out_start_rate_cb) {
        conf->move_out_start_rate = (double)cmd->data.dvalue;
        return NULL;
}

static DOTCONF_CB(move_out_stop_rate_cb) {
        conf->move_out_stop_rate = (double)cmd->data.dvalue;
        return NULL;
}

static DOTCONF_CB(out_q_max_size_cb) {
        conf->out_q_max_size = (size_t)cmd->data.value;
        return NULL;
}

static DOTCONF_CB(in_q_max_size_cb) {
        conf->in_q_max_size = (size_t)cmd->data.value;
        return NULL;
}

static DOTCONF_CB(logger_cb) {
        for (int i = 0; i < log_count; i++) {
                if (strcmp(cmd->data.str, log_str[i]) == 0) {
                        log = log_arr[i];
                        return NULL;
                }
        }

        return "unsupported logging framework";
}

static DOTCONF_CB(remote_store_protocol_cb) {
        if (strcmp(cmd->data.str, protocol_str[e_s3]) == 0) {
                conf->ops = s3_ops;
                strcpy(conf->remote_store_protocol, cmd->data.str);
        } else {
                return "unsupported remote store protocol";
        }

        return NULL;
}

static DOTCONF_CB(transfer_protocol_cb) {
        strcpy(conf->transfer_protocol, cmd->data.str);
        return NULL;
}

static DOTCONF_CB(s3_default_hostname_cb) {
        strcpy(conf->s3_default_hostname, cmd->data.str);
        return NULL;
}

static DOTCONF_CB(s3_bucket_cb) {
        strcpy(conf->s3_bucket, cmd->data.str);
        return NULL;
}

static DOTCONF_CB(s3_access_key_id_cb) {
        strcpy(conf->s3_access_key_id, cmd->data.str);
        return NULL;
}

static DOTCONF_CB(s3_secret_access_key_cb) {
        strcpy(conf->s3_secret_access_key, cmd->data.str);
        return NULL;
}


/*******************************************************************************
* Dotconf's options' definitions.                                              *
*******************************************************************************/
static const configoption_t options[] = {
        /* General section */
        { beg_General_section_str,       ARG_NONE,   beg_General_section_cb,       NULL, CTX_ALL                    },
        { "FsMountPoint",                ARG_STR,    fs_mount_point_cb,            NULL, SECTION_CTX(General)       },
        { "LoggingFramework",            ARG_STR,    logger_cb,                    NULL, SECTION_CTX(General)       },
        { "RemoteStoreProtocol",         ARG_STR,    remote_store_protocol_cb,     NULL, SECTION_CTX(General)       },
        { end_General_section_str,       ARG_NONE,   end_General_section_cb,       NULL, CTX_ALL                    },

        /* Internal section */
        { beg_Internal_section_str,      ARG_NONE,   beg_Internal_section_cb,      NULL, CTX_ALL                    },
        { "ScanfsIterTimeoutSec",        ARG_INT,    scanfs_iter_tm_sec_cb,        NULL, SECTION_CTX(Internal)      },
        { "ScanfsMaximumFailures",       ARG_INT,    scanfs_max_fails_cb,          NULL, SECTION_CTX(Internal)      },
        { "MoveFileMaximumFailures",     ARG_INT,    move_file_max_fails_cb,       NULL, SECTION_CTX(Internal)      },
        { "MoveOutStartRate",            ARG_DOUBLE, move_out_start_rate_cb,       NULL, SECTION_CTX(Internal)      },
        { "MoveOutStopRate",             ARG_DOUBLE, move_out_stop_rate_cb,        NULL, SECTION_CTX(Internal)      },
        { "OutQueueMaxSize",             ARG_INT,    out_q_max_size_cb,            NULL, SECTION_CTX(Internal)      },
        { "InQueueMaxSize",              ARG_INT,    in_q_max_size_cb,             NULL, SECTION_CTX(Internal)      },
        { end_Internal_section_str,      ARG_NONE,   end_Internal_section_cb,      NULL, CTX_ALL                    },

        /* S3RemoteStore section */
        { beg_S3RemoteStore_section_str, ARG_NONE,   beg_S3RemoteStore_section_cb, NULL, CTX_ALL                    },
        { "S3DefaultHostname",           ARG_STR,    s3_default_hostname_cb,       NULL, SECTION_CTX(S3RemoteStore) },
        { "S3Bucket",                    ARG_STR,    s3_bucket_cb,                 NULL, SECTION_CTX(S3RemoteStore) },
        { "S3AccessKeyId",               ARG_STR,    s3_access_key_id_cb,          NULL, SECTION_CTX(S3RemoteStore) },
        { "S3SecretAccessKey",           ARG_STR,    s3_secret_access_key_cb,      NULL, SECTION_CTX(S3RemoteStore) },
        { "TransferProtocol",            ARG_STR,    transfer_protocol_cb,         NULL, SECTION_CTX(S3RemoteStore) },
        { end_S3RemoteStore_section_str, ARG_NONE,   end_S3RemoteStore_section_cb, NULL, CTX_ALL                    },

        LAST_OPTION
};


/**
 * @brief get_conf Get pointer to configuration.
 *
 * @return Pointer to conf_t or NULL if read_conf() has not been
 *         invoked yet or failed.
 */
inline conf_t *get_conf() {
        return conf;
}


/**
 * @brief get_log Get pointer to logging framework.
 *
 * @return Pointer to log_t or NULL if read_conf() has not been
 *         invoked yet or failed.
 */
inline log_t *get_log() {
        return log;
}


/**
 * @brief get_ops Get pointer to operatons data-structure.
 *
 * @return Pointer to ops_t or NULL if read_conf() has not been
 *         invoked yet or failed.
 */
inline ops_t *get_ops() {
        return ops;
}


/**
 * @brief init_dependent_members Initialize remaining members of configuration.
 *
 * @return  0: when all system calls were successful
 *         -1: when at least one system call has failed
 */
static int init_dependent_members(void) {
        /* pathconf will not change errno and will return -1 if requested
           resource does not have limit */
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
 * @brief read_conf Read configuration from file.
 *
 * @note Does not handle the case when line length exceeded LINE_MAX limit.
 * @note Does not handle the case when file contains duplicate keys.
 *
 * @param[in] conf_path Path to a configuration file.
 *                      If NULL then use default configuration.
 *
 * @return  0: configuration has successfully been read from file
 *         -1: errors is system calls occured or configuration file is invalid
 */
int read_conf(const char *conf_path) {
        if ((conf != NULL) || (log != NULL) || (ops != NULL)) {
                return -1;
        }

        conf = malloc(sizeof(conf_t));
        log  = malloc(sizeof(log_t));
        ops  = malloc(sizeof(ops_t));
        if ((conf == NULL) || (log == NULL) || (ops == NULL)) {
                free(conf);
                free(log);
                free(ops);

                return -1;
        }

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
