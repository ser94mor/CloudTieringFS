#define _POSIX_C_SOURCE    200112L    /* required for strerror_r() */

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <attr/xattr.h>
#include <sys/stat.h>
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
        xattr_location_t location = 0;

        if (getxattr(path, xattr, &location, size) == -1) {
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

        return (location == LOCAL_STORE);
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
                /* get front element in the queue */
                size_t it_sz = 0;
                char *path = queue_front(queue, &it_sz);
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
                        if (getconf()->ops.move_file_out(path) == -1) {
                                sprintf(err_buf, "failed to move file %s out", path);
                                goto err;
                        }
                } else {
                        local_flag = 0;

                        /* remote files should be moved in */
                        if (getconf()->ops.move_file_in(path) == -1) {
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
        conf_t *conf = getconf();

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
    struct s3_put_object_callback_data *data = (struct s3_put_object_callback_data *)callback_data;

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

        /* set call back data type */
        struct s3_cb_data callback_data = {
                .type = s3_cb_PUT_OBJECT,
                .error_details = { 0 },
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
        struct s3_put_object_callback_data put_object_data;
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
        conf_t *conf = getconf();

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
        if (s3_put_object(path) == -1) {
                LOG(ERROR, "failed to put object into remote store for file %s", path);
                return -1;
        }
        LOG(DEBUG, "file %s was uploaded to remote store successfully", path);

        const char *key = xattr_str[s3_object_id];
        const char *value = path;

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

        return 0;
}

int s3_move_file_in(const char *path) {
        /* TODO: implement this method */
        return -1;
}

void s3_term_remote_store_access(void) {
        S3_deinitialize();
}
