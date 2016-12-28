/**
 * Copyright (C) 2016  Sergey Morozov <sergey94morozov@gmail.com>
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

#define _POSIX_C_SOURCE    200112L    /* required for strerror_r() */
#define _XOPEN_SOURCE 500    /* needed to use nftw() */

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <ftw.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <attr/xattr.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libs3.h>

#include "cloudtiering.h"

/* thread-local buffer for error messages;
 * 1024 is more than enough to store errno descriptions */
#define ERR_MSG_BUF_LEN    1024
static __thread char err_buf[ERR_MSG_BUF_LEN];

/*
 * Definitions related to extended attributes used to store
 * information about remote storage object location.
 */
#define XATTR_NAMESPACE             "trusted"
#define LOCAL_STORE                 0x01
#define REMOTE_STORE                0x10

/*
 * - S3_MAX_KEY_SIZE is defined in libs3.h
 * - "locked" is extended attribute indicating that some thread is currently working with this file
 */
#define FOREACH_XATTR(action) \
        action(s3_object_id, S3_MAX_KEY_SIZE, char *) \
        action(location, sizeof(unsigned char), unsigned char) \
        action(locked, 0, void)

#define XATTR_ENUM(key, size, type)       key,
#define XATTR_STR(key, size, type)        XATTR_NAMESPACE "." #key,
#define XATTR_MAX_SIZE(key, size, type)   size,
#define XATTR_TYPEDEF(key, size, type)    typedef type xattr_##key##_t;

/* declare enum of extended attributes */
enum xattr_enum {
        FOREACH_XATTR(XATTR_ENUM)
};

/* declare array of extended attributes' keys */
static char *xattr_str[] = {
        FOREACH_XATTR(XATTR_STR)
};

/* declare array of extended attributes' sizes */
static size_t xattr_max_size[] = {
        FOREACH_XATTR(XATTR_MAX_SIZE)
};

/* declare types of extended attributes' values */
FOREACH_XATTR(XATTR_TYPEDEF);

static int is_file_locked(const char *path) {
        int ret = getxattr(path, xattr_str[locked], NULL, 0);

        if (ret == -1) {
                if (errno == ENOATTR) {
                        return 0; /* false; not locked */
                }

                /* strerror_r() with very low probability can fail; ignore such failures */
                strerror_r(errno, err_buf, ERR_MSG_BUF_LEN);

                LOG(ERROR,
                    "failed to get extended attribute %s of file %s [reason: %s]",
                    xattr_str[locked],
                    path,
                    err_buf);

                return -1; /* error indication */
        }

        return 1; /* true; file is locked */
}

static int lock_file(const char *path) {
        int ret = setxattr(path, xattr_str[locked], NULL, 0, XATTR_CREATE);

        if (ret == -1) {
                /* strerror_r() with very low probability can fail; ignore such failures */
                strerror_r(errno, err_buf, ERR_MSG_BUF_LEN);

                LOG(ERROR,
                    "failed to set extended attribute %s to file %s [reason: %s]",
                    xattr_str[locked],
                    path,
                    err_buf);

                return -1;
        }

        return 0;
}

static int unlock_file(const char *path) {
        int ret = removexattr(path, xattr_str[locked]);

        if (ret == -1) {
                /* strerror_r() with very low probability can fail; ignore such failures */
                strerror_r(errno, err_buf, ERR_MSG_BUF_LEN);

                LOG(ERROR,
                    "failed to remove extended attribute %s of file %s [reason: %s]",
                    xattr_str[locked],
                    path,
                    err_buf);

                return -1;
        }

        return 0;
}

/**
 * @brief is_file_local Check whether file data is on local store or on remote.
 * @param[in] path Path to target file.
 * @return 0 is file data on remote store and >0 when file data on local store; -1 on error
 */
static int is_file_local(const char *path) {
        size_t size = xattr_max_size[location];
        const char *xattr = xattr_str[location];
        xattr_location_t loc = 0;

        if (getxattr(path, xattr, &loc, size) == -1) {
                if (errno == ENOATTR) {
                        return 1; /* true */
                }

                /* strerror_r() with very low probability can fail; ignore such failures */
                strerror_r(errno, err_buf, ERR_MSG_BUF_LEN);

                LOG(ERROR,
                    "failed to get extended attribute %s of file %s [reason: %s]",
                    xattr,
                    path,
                    err_buf);

                return -1; /* error indication */
        }

        return !!(loc == LOCAL_STORE);
}

/**
 * @brief is_valid_path Checks if path is valid - exists and is is regular file.
 * @param[in] path Path to target file.
 * @return 0 if path is not valid and non-0 if path is valid.
 */
static int is_valid_path(const char *path) {
        struct stat path_stat;
        if ((stat(path, &path_stat) == -1) || !S_ISREG(path_stat.st_mode)) {
                return 0; /* false */
        }

        return 1; /* true */
}

/**
 * @brief move_file Move file on the front of the queue in or out (obvious from the file attributes).
 * @param[in] queue Queue with candidate files for moving in or out.
 * @return -1 on failure; 0 on success
 */
int move_file(queue_t *queue) {
        if (!queue_empty(queue)) {
                /* double queue_front invocation does not lead to violation of
                   thread-safeness since given queue has only one consumer
                   (this thread) */

                /* get item size of front element */
                size_t it_sz = 0;
                queue_front(queue, NULL, &it_sz);

                /* get front element of the queue */
                char path[it_sz];
                queue_front(queue, path, &it_sz);

                if (path == NULL) {
                        strcpy(err_buf, "failed to get front element of queue");
                        goto err;
                }

                if (!is_valid_path(path)) {
                        sprintf(err_buf, "path %s is not valid", path);
                        goto err;
                }

                int local_flag = 0;
                if (is_file_local(path)) {
                        local_flag = 1;

                        /* local files should be moved out */
                        if (get_conf()->ops.move_file_out(path) == -1) {
                                sprintf(err_buf, "failed to move file %s out", path);
                                goto err;
                        }
                } else {
                        local_flag = 0;

                        /* remote files should be moved in */
                        if (get_conf()->ops.move_file_in(path) == -1) {
                                sprintf(err_buf, "failed to move file %s in", path);
                                goto err;
                        }
                }

                queue_pop(queue);
                LOG(INFO, "file %s successfully moved %s", path, (local_flag ? "in" : "out"));
        } else {
                //LOG(DEBUG, "queue is empty; nothing to move");
        }

        /* return successfully if there are no files to move or move in/out operation was successful */
        return 0;

        err:
        queue_pop(queue);
        LOG(ERROR, "failed to move file [reason: %s]", err_buf);
        return -1;
}


/**************************
 * S3 Protocol Operations *
 **************************/
/* how many times retriable s3 operation should be retried */
#define S3_RETRIES    5

/*  context for working with objects within a bucket. Initialized only once. */
static S3BucketContext g_bucket_context;

/* enum of s3 operation types; used to determine callback behaviour */
enum s3_cb_type {
        s3_cb_TEST_BUCKET,
        s3_cb_CREATE_BUCKET,
        s3_cb_PUT_OBJECT,
        s3_cb_GET_OBJECT,
};

/* used as in and out a parameter for s3_response_complete_callback() */
struct s3_cb_data {
        enum s3_cb_type type; /* method identifier */
        S3Status status; /* request status */
        char error_details[4096];
        void *data; /* some data */
};


/* used in s3_put_object_data_callback(); convinient to give file's stuff as a single argument */
struct s3_put_object_callback_data {
    FILE *infile;
    uint64_t content_length;
};

static S3Status
s3_response_properties_callback(const S3ResponseProperties *properties, void *callbackData) {
        return S3StatusOK;
}

static void
s3_response_complete_callback(S3Status status, const S3ErrorDetails *error, void *callback_data) {
        struct s3_cb_data *cb_d = (struct s3_cb_data *)callback_data;
        cb_d->status = status;

        int len = 0;
        if (error && error->message) {
                len += snprintf(&(cb_d->error_details[len]), sizeof(cb_d->error_details) - len,
                                "  Message: %s\n", error->message);
        }
        if (error && error->resource) {
                len += snprintf(&(cb_d->error_details[len]), sizeof(cb_d->error_details) - len,
                                "  Resource: %s\n", error->resource);
        }
        if (error && error->furtherDetails) {
                len += snprintf(&(cb_d->error_details[len]), sizeof(cb_d->error_details) - len,
                                "  Further Details: %s\n", error->furtherDetails);
        }
        if (error && error->extraDetailsCount) {
                len += snprintf(&(cb_d->error_details[len]), sizeof(cb_d->error_details) - len,
                                "%s", "  Extra Details:\n");
                int i;
                for (i = 0; i < error->extraDetailsCount; i++) {
                len += snprintf(&(cb_d->error_details[len]),
                                sizeof(cb_d->error_details) - len, "    %s: %s\n",
                                error->extraDetails[i].name,
                                error->extraDetails[i].value);
                }
        }
}

static S3ResponseHandler g_response_handler = {
        .propertiesCallback = &s3_response_properties_callback,
        .completeCallback   = &s3_response_complete_callback
};

static void s3_init_globals(void) {
        conf_t *conf = get_conf();

        S3Protocol transfer_protocol = strcmp(conf->transfer_protocol, "http") == 0 ?
                                              S3ProtocolHTTP : S3ProtocolHTTPS;

        g_bucket_context.hostName        = conf->s3_default_hostname;
        g_bucket_context.bucketName      = conf->s3_bucket;
        g_bucket_context.protocol        = transfer_protocol;
        g_bucket_context.uriStyle        = S3UriStylePath;
        g_bucket_context.accessKeyId     = conf->s3_access_key_id;
        g_bucket_context.secretAccessKey = conf->s3_secret_access_key;
        g_bucket_context.securityToken   = NULL;
}

static int s3_bucket_exists(void) {
        /* create bucket if it does not already exist */
        int retries = S3_RETRIES;

        /* prepare callback data structure */
        struct s3_cb_data callback_data = {
                .type = s3_cb_TEST_BUCKET,
        };

        /* test the existance of bucket */
        char location_constraint[64];
        do {
                S3_test_bucket(g_bucket_context.protocol,
                               S3UriStylePath,
                               g_bucket_context.accessKeyId,
                               g_bucket_context.secretAccessKey,
                               NULL,
                               g_bucket_context.hostName,
                               g_bucket_context.bucketName,
                               sizeof(location_constraint),
                               location_constraint,
                               NULL,
                               &g_response_handler,
                               &callback_data);
        } while (S3_status_is_retryable(callback_data.status) && --retries);

        /* true if exists; if not exist and on error - false */
        return (callback_data.status == S3StatusOK);
}

static int s3_create_bucket(void) {
        /* create bucket if it does not already exist */
        int retries = S3_RETRIES;

        struct s3_cb_data callback_data = {
                .type = s3_cb_CREATE_BUCKET,
        };

        /* create bucket if it does not already exist */
        do {
                S3_create_bucket(g_bucket_context.protocol,
                                 g_bucket_context.accessKeyId,
                                 g_bucket_context.secretAccessKey,
                                 NULL,
                                 g_bucket_context.hostName,
                                 g_bucket_context.bucketName,
                                 S3CannedAclPrivate,
                                 NULL,
                                 NULL,
                                 &g_response_handler,
                                 &callback_data);
        } while (S3_status_is_retryable(callback_data.status) && --retries);

        /* fail on any error */
        if (callback_data.status != S3StatusOK) {
                return -1;
        }

        return 0;
}

static int s3_put_object_data_callback(int buffer_size, char *buffer, void *callback_data) {
    struct s3_put_object_callback_data *data =
        (struct s3_put_object_callback_data *)(((struct s3_cb_data *)callback_data)->data);

    int ret = 0;

    if (data->content_length) {
        int toRead = ((data->content_length > (unsigned) buffer_size) ?
                         (unsigned)buffer_size : data->content_length);
        ret = fread(buffer, 1, toRead, data->infile);
    }
    data->content_length -= ret;

    return ret;
}

static int s3_put_object(const char *path) {
        int retries = S3_RETRIES;
        int ret = 0; /* success by default */
        struct s3_put_object_callback_data put_object_data;

        /* set call back data type */
        struct s3_cb_data callback_data = {
                .type = s3_cb_PUT_OBJECT,
                .error_details = { 0 },
                .data = &put_object_data,
        };

        /* stat structure for target file to get content length */
        struct stat statbuf;
        if (stat(path, &statbuf) == -1) {
                /* use thead safe version of strerror() */
                if (strerror_r(errno, err_buf, ERR_MSG_BUF_LEN) == -1) {
                        err_buf[0] = '\0'; /* very unlikely */
                }

                LOG(ERROR, "failed to stat file %s: %s", path, err_buf);

                return -1;
        }

        /* initialize structure used during  */
        put_object_data.content_length = statbuf.st_size;
        if (!(put_object_data.infile = fopen(path, "r"))) {
                /* use thead safe version of strerror() */
                if (strerror_r(errno, err_buf, ERR_MSG_BUF_LEN) == -1) {
                        err_buf[0] = '\0'; /* very unlikely */
                }

                LOG(ERROR, "failed to open input file %s: %s", path, err_buf);

                return -1;
        }

        S3PutObjectHandler put_object_handler = {
                .responseHandler = g_response_handler,
                .putObjectDataCallback = &s3_put_object_data_callback,
        };

        do {
                S3_put_object(&g_bucket_context,
                              path,
                              put_object_data.content_length,
                              NULL,
                              NULL,
                              &put_object_handler,
                              &callback_data);
        } while(S3_status_is_retryable(callback_data.status) && --retries);

        /* fail on any error */
        if (callback_data.status != S3StatusOK) {
                LOG(ERROR, "S3_put_object() failed with error: %s", S3_get_status_name(callback_data.status));
                LOG(ERROR, callback_data.error_details);
                ret = -1;
        }

        if (fclose(put_object_data.infile)) {
                /* this is usually caused by very serious system error */

                /* use thead safe version of strerror() */
                if (strerror_r(errno, err_buf, ERR_MSG_BUF_LEN) == -1) {
                        err_buf[0] = '\0'; /* very unlikely */
                }

                LOG(ERROR, "failed to close file %s: %s", path, err_buf);
                ret = -1;
        }

        return ret;
}

int s3_init_remote_store_access(void) {
        conf_t *conf = get_conf();

        S3Status status = S3_initialize(NULL, S3_INIT_ALL, conf->s3_default_hostname);


        if (status == S3StatusOK) {
                LOG(DEBUG, "S3_initialize() executed successfully");
        } else if (status == S3StatusUriTooLong) {
                LOG(ERROR, "default s3 hostname %s is longer than %d (error: %s)",
                    conf->s3_default_hostname, S3_MAX_HOSTNAME_SIZE, S3_get_status_name(status));
                return -1;
        } else if (status == S3StatusInternalError) {
                LOG(ERROR, "libs3 dependent libraries could not be initialized (error: %s)",
                    S3_get_status_name(status));
                return -1;
        } else if (status == S3StatusOutOfMemory) {
                LOG(ERROR, "out of memory; unable to initialize libs3 (error: %s)", S3_get_status_name(status));
                return -1;
        } else {
                LOG(ERROR, "unknown libs3 error (error: %d)", S3_get_status_name(status));
                return -1;
        }

        /* validates bucket name, not its availability */
        status = S3_validate_bucket_name(conf->s3_bucket, S3UriStylePath);
        if (status != S3StatusOK) {
                LOG(ERROR, "not valid bucket name for path bucket style (error: %s)",
                    S3_get_status_name(status));
                return -1;
        }

        s3_init_globals();

        /* create bucket if it does not exist, if it already exists do nothing */
        if (!s3_bucket_exists()) {
                if (s3_create_bucket() == -1) {
                        LOG(ERROR, "failed to create bucket %s", conf->s3_bucket);
                        return -1;
                }
                LOG(DEBUG, "bucket %s created successfully", conf->s3_bucket);
        }

        LOG(INFO, "access to remote store setup successfully");

        return 0;
}

int s3_move_file_out(const char *path) {
        if (lock_file(path) == -1) {
                return -1;
        }

        if (s3_put_object(path) == -1) {
                LOG(ERROR, "failed to put object into remote store for file %s", path);
                /* TODO: unlock_file() in all error places */
                return -1;
        }
        LOG(DEBUG, "file %s was uploaded to remote store successfully", path);

        const char *key = xattr_str[s3_object_id];
        xattr_s3_object_id_t value = (xattr_s3_object_id_t)path;

        if (setxattr(path, key, value, strlen(value) + 1, XATTR_CREATE) == -1) {
                /* strerror_r() with very low probability can fail; ignore such failures */
                strerror_r(errno, err_buf, ERR_MSG_BUF_LEN);

                LOG(ERROR,
                    "failed to set extended attribute %s with value %s to file %s [reason: %s]",
                    key,
                    path,
                    path,
                    err_buf);

                return -1;
        }

        key = xattr_str[location];
        xattr_location_t location_val = REMOTE_STORE;

        if (setxattr(path, key, &location_val, xattr_max_size[location], XATTR_CREATE) == -1) {
                /* strerror_r() with very low probability can fail; ignore such failures */
                strerror_r(errno, err_buf, ERR_MSG_BUF_LEN);

                LOG(ERROR,
                    "failed to set extended attribute %s with value %s to file %s [reason: %s]",
                    key,
                    path,
                    path,
                    err_buf);

                return -1;
        }

        int ret = 0;
        int fd;
        if ((fd = open(path, O_TRUNC | O_WRONLY)) == -1) {
                /* strerror_r() with very low probability can fail; ignore such failures */
                strerror_r(errno, err_buf, ERR_MSG_BUF_LEN);

                LOG(ERROR,
                    "failed to execute open() to truncate file %s [reason: %s]",
                    path,
                    err_buf);

                ret = -1;
        }

        if (close(fd) == -1) {
                /* strerror_r() with very low probability can fail; ignore such failures */
                strerror_r(errno, err_buf, ERR_MSG_BUF_LEN);

                LOG(ERROR,
                    "failed to execute close() on file %s [reason: %s]",
                    path,
                    err_buf);

                ret = -1;
        }

        if (unlock_file(path) == -1) {
                ret = -1;
        }

        return ret;
}

int s3_move_file_in(const char *path) {
        /* TODO: implement this method */
        return -1;
}

void s3_term_remote_store_access(void) {
        S3_deinitialize();
}

/*******************
 * Scan filesystem *
 * *****************/
/* queue might be full and we need to perform several attempts giving a chance
 * for other threads to pop elements from queue */
#define QUEUE_PUSH_RETRIES              5
#define QUEUE_PUSH_ATTEMPT_SLEEP_SEC    1

static queue_t *in_queue  = NULL;
static queue_t *out_queue = NULL;

static int update_evict_queue(const char *fpath, const struct stat *sb,  int typeflag, struct FTW *ftwbuf) {
        int attempt = 0;

        struct stat path_stat;
        if (stat(fpath, &path_stat) == -1) {
                return 0; /* just continue with the next files; non-zero will cause failure of nftw() */
        }

        if (S_ISREG(path_stat.st_mode) &&
            is_file_local(fpath) &&
            !is_file_locked(fpath) &&
            (path_stat.st_atime + 600) < time(NULL)) {
                do {
                        if (queue_push(out_queue, (char *)fpath, strlen(fpath) + 1) == -1) {
                                ++attempt;
                                continue;
                        }
                        LOG(DEBUG, "file '%s' pushed to out queue", fpath);
                        break;
                } while ((attempt < QUEUE_PUSH_RETRIES) && (queue_full((queue_t *)out_queue) > 0));
        }

        return 0;
}

int scanfs(queue_t *in_q, queue_t *out_q) {
        conf_t *conf = get_conf();

        in_queue  = in_q;
        out_queue = out_q;

        /* set maximum number of open files for nftw to a half of descriptor table size for current process */
        struct rlimit rlim;
        if (getrlimit(RLIMIT_NOFILE, &rlim) == -1) {
                return -1;
        }

        /* rlim_cur value will be +1 higher than actual according to documentation */
        if (rlim.rlim_cur <= 2) {
                /* there are 1 or less descriptors available */
                return -1;
        }
        int nopenfd = rlim.rlim_cur / 2;

        /* stay within filesystem and do not follow symlinks */
        int flags = FTW_MOUNT | FTW_PHYS;

        if (nftw(conf->fs_mount_point, update_evict_queue, nopenfd, flags) == -1) {
                return -1;
        }

        return 0;
}
