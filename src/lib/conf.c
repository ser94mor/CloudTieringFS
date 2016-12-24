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
#include "ops.h"

static conf_t *conf = NULL;
static log_t  *log  = NULL;

static DOTCONF_CB(fs_mount_point);
static DOTCONF_CB(scanfs_iter_tm_sec);
static DOTCONF_CB(scanfs_max_fails);
static DOTCONF_CB(move_file_max_fails);
static DOTCONF_CB(move_out_start_rate);
static DOTCONF_CB(move_out_stop_rate);
static DOTCONF_CB(out_q_max_size);
static DOTCONF_CB(in_q_max_size);
static DOTCONF_CB(logger);
static DOTCONF_CB(s3_default_hostname);
static DOTCONF_CB(s3_bucket);
static DOTCONF_CB(s3_access_key_id);
static DOTCONF_CB(s3_secret_access_key);
static DOTCONF_CB(remote_store_protocol);
static DOTCONF_CB(transfer_protocol);


/*
 * Common macroses.
 */
#define GENERATE_ENUM(item) item,

#define GENERATE_STR_ARRAY(item) #item,


/*
 * Supported remote store protocol definitions.
 */
#define DECL_PROTOCOL_OPS(protocol) \
        static ops_t protocol##_ops = { \
                .init_remote_store_access = protocol##_init_remote_store_access, \
                .move_file_in             = protocol##_move_file_in, \
                .move_file_out            = protocol##_move_file_out, \
                .term_remote_store_access = protocol##_term_remote_store_access \
        };

#define FOREACH_PROTOCOL(action) \
        action(s3)

FOREACH_PROTOCOL(DECL_PROTOCOL_OPS);

/* following enum values are indexes to string repserentations */
enum protocol_enum {
        FOREACH_PROTOCOL(GENERATE_ENUM)
};

static const char *protocol_str[] = {
        FOREACH_PROTOCOL(GENERATE_STR_ARRAY)
};


/*******************************************************************************
* Logging frameworks' definitions.                                             *
*******************************************************************************/

/* declaration of structures for each type of supported logging framework */
LOGGERS(DECLARE_LOGGER, SEMICOLON);

/* string names of logging frameworks */
static const char *log_type_str[] = {
        LOGGERS(STRINGIFY, COMMA),
};

/* array of pointers to logging frameworks' structures */
static log_t *log_arr[] = {
        LOGGERS(LOGGER_ADDR, COMMA),
};

/* number of supported logging frameworks */
static const size_t log_count = LOGGERS(MAP_TO_ONE, PLUS);


/*
 * Section definitions.
 */
/* CTX_ALL expand to 0, that is why '__COUNTER__ + 1' value is used below */
#define DECL_SECT(name) \
        static const char beg_##name##_sect[] = "<"  #name ">", \
                          end_##name##_sect[] = "</" #name ">"; \
        static DOTCONF_CB(open_##name##_sect) { \
                return NULL; \
        } \
        static DOTCONF_CB(close_##name##_sect) { \
                return NULL; \
        }

#define GENERATE_ENUM_CUSTOM(item) ctx_##item = __COUNTER__ + 1,

#define FOREACH_SECT(action) \
        action(General) \
        action(Internal) \
        action(S3RemoteStore)

FOREACH_SECT(DECL_SECT);

enum sect_enum {
        FOREACH_SECT(GENERATE_ENUM_CUSTOM)
};


/*
 * Option definitions.
 */
static const configoption_t options[] = {
        /* General section */
        { beg_General_sect,          ARG_NONE,   open_General_sect,        NULL, CTX_ALL          },
        { "FsMountPoint",            ARG_STR,    fs_mount_point,           NULL, ctx_General      },
        { "LoggingFramework",        ARG_STR,    logger,                   NULL, ctx_General      },
        { "RemoteStoreProtocol",     ARG_STR,    remote_store_protocol,    NULL, ctx_General      },
        { end_General_sect,          ARG_NONE,   close_General_sect,       NULL, CTX_ALL          },

        /* Internal section */
        { beg_Internal_sect,         ARG_NONE,   open_Internal_sect,       NULL, CTX_ALL           },
        { "ScanfsIterTimeoutSec",    ARG_INT,    scanfs_iter_tm_sec,       NULL, ctx_Internal      },
        { "ScanfsMaximumFailures",   ARG_INT,    scanfs_max_fails,         NULL, ctx_Internal      },
        { "MoveFileMaximumFailures", ARG_INT,    move_file_max_fails,      NULL, ctx_Internal      },
        { "MoveOutStartRate",        ARG_DOUBLE, move_out_start_rate,      NULL, ctx_Internal      },
        { "MoveOutStopRate",         ARG_DOUBLE, move_out_stop_rate,       NULL, ctx_Internal      },
        { "OutQueueMaxSize",         ARG_INT,    out_q_max_size,           NULL, ctx_Internal      },
        { "InQueueMaxSize",          ARG_INT,    in_q_max_size,            NULL, ctx_Internal      },
        { end_Internal_sect,         ARG_NONE,   close_Internal_sect,      NULL, CTX_ALL           },

        /* S3RemoteStore section */
        { beg_S3RemoteStore_sect,    ARG_NONE,   open_S3RemoteStore_sect,  NULL, CTX_ALL           },
        { "S3DefaultHostname",       ARG_STR,    s3_default_hostname,      NULL, ctx_S3RemoteStore },
        { "S3Bucket",                ARG_STR,    s3_bucket,                NULL, ctx_S3RemoteStore },
        { "S3AccessKeyId",           ARG_STR,    s3_access_key_id,         NULL, ctx_S3RemoteStore },
        { "S3SecretAccessKey",       ARG_STR,    s3_secret_access_key,     NULL, ctx_S3RemoteStore },
        { "TransferProtocol",        ARG_STR,    transfer_protocol,        NULL, ctx_S3RemoteStore },
        { end_S3RemoteStore_sect,    ARG_NONE,   close_S3RemoteStore_sect, NULL, CTX_ALL           },
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

static DOTCONF_CB(scanfs_max_fails) {
        conf->scanfs_max_fails = (int)cmd->data.value;
        return NULL;
}

static DOTCONF_CB(move_file_max_fails) {
        conf->move_file_max_fails = (int)cmd->data.value;
        return NULL;
}

static DOTCONF_CB(move_out_start_rate) {
        conf->move_out_start_rate = (double)cmd->data.dvalue;
        return NULL;
}

static DOTCONF_CB(move_out_stop_rate) {
        conf->move_out_stop_rate = (double)cmd->data.dvalue;
        return NULL;
}

static DOTCONF_CB(out_q_max_size) {
        conf->out_q_max_size = (size_t)cmd->data.value;
        return NULL;
}

static DOTCONF_CB(in_q_max_size) {
        conf->in_q_max_size = (size_t)cmd->data.value;
        return NULL;
}

static DOTCONF_CB(logger) {
        for (int i = 0; i < log_count; i++) {
                if (strcmp(cmd->data.str, log_type_str[i]) == 0) {
                        log = log_arr[i];
                        return NULL;
                }
        }

        return "unsupported logging framework";
}

static DOTCONF_CB(remote_store_protocol) {
        if (strcmp(cmd->data.str, protocol_str[s3]) == 0) {
                conf->ops = s3_ops;
                strcpy(conf->remote_store_protocol, cmd->data.str);
        } else {
                return "unsupported remote store protocol";
        }

        return NULL;
}

static DOTCONF_CB(transfer_protocol) {
        strcpy(conf->transfer_protocol, cmd->data.str);
        return NULL;
}

static DOTCONF_CB(s3_default_hostname) {
        strcpy(conf->s3_default_hostname, cmd->data.str);
        return NULL;
}

static DOTCONF_CB(s3_bucket) {
        strcpy(conf->s3_bucket, cmd->data.str);
        return NULL;
}

static DOTCONF_CB(s3_access_key_id) {
        strcpy(conf->s3_access_key_id, cmd->data.str);
        return NULL;
}

static DOTCONF_CB(s3_secret_access_key) {
        strcpy(conf->s3_secret_access_key, cmd->data.str);
        return NULL;
}

/**
 * @brief getconf Get pointer to configuration.
 * @return Pointer to conf_t or NULL if readconf has not been
 *         invoked yet or failed.
 */
inline conf_t *getconf() {
        return conf;
}

/**
 * @brief get_log Get pointer to logging framework.
 * @return Pointer to log_t or NULL if readconf has not been
 *         invoked yet or failed.
 */
inline log_t *get_log() {
        return log;
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

        conf = malloc(sizeof(conf_t));
        if (conf == NULL) {
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
