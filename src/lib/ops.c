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

/* S3_MAX_KEY_SIZE is defined in libs3.h */
#define FOREACH_XATTR(action) \
        action(s3_object_id, S3_MAX_KEY_SIZE) \
        action(location, sizeof(unsigned char))

#define XATTR_ENUM(key, size)        key,
#define XATTR_STR(key, size)         XATTR_NAMESPACE "." #key,
#define XATTR_MAX_SIZE(key, size)    size,

enum xattr_enum {
        FOREACH_XATTR(XATTR_ENUM)
};

static char *xattr_str[] = {
        FOREACH_XATTR(XATTR_STR)
};

static size_t xattr_max_size[] = {
        FOREACH_XATTR(XATTR_MAX_SIZE)
};

/**
 * @brief set_xattr Wrapper around setxattr() function with error handling.
 * @param[in] path  Path to the target file.
 * @param[in] key   Name of the extended attribute.
 * @param[in] value Value to be set to extended attribute.
 * @return 0 on success, -1 on failure
 */
static int set_xattr(const char *path, const char *key, const char *value, const size_t size) {
         /* create extended attribute or replace existing */
        int ret = setxattr(path, key, value, size, 0);
        if (ret == -1) {
                /* use thead safe version of strerror() */
                if (strerror_r(errno, err_buf, ERR_MSG_BUF_LEN) == -1) {
                        err_buf[0] = '\0'; /* very unlikely */
                }

                LOG(DEBUG, "failed to set extended attribute for %s [key: %s; value: %s]; reason: %s",
                    path, key, value, err_buf);

                return -1;
        }

        return 0;
}

/**
 * @brief get_xattr Wrapper around getxattr() function with error handling.
 * @param[in]     path      Path to the target file.
 * @param[in]     key       Name of the extended attribute.
 * @param[out]    value_buf Buffer that will contain value of extended attribute on success.
 * @param[in,out] size      Size of probvided buffer. Will be set to 0 in case of attribute non-existance.
 * @return 0 on success, -1 on failure
 */
static int get_xattr(const char *path, const char *key, char *value_buf, size_t *size) {
        ssize_t ret = getxattr(path, key, value_buf, *size);
        if (ret == -1) {
                if (errno == ENOATTR) {
                        /* attribute does not exist or process does not have an access to it */
                        *size = 0;
                        return 0;
                }

                /* using thead safe version of strerror() */
                if (strerror_r(errno, err_buf, ERR_MSG_BUF_LEN) == -1) {
                        err_buf[0] = '\0'; /* very unlikely */
                }

                LOG(DEBUG, "failed to get extended attribute for %s [key: %s]; reason: %s",
                    path, key, err_buf);

                return -1;
        }

        return 0;
}

/**
 * @brief remove_xattr Wrapper around removexattr() function with error handling.
 * @param[in] path Path to the target file.
 * @param[in] key  Name of the extended attribute.
 * @return 0 on success, -1 on failure
 */
static int remove_xattr(const char *path, const char *key) {
        /* handle all possible errors here */
        int ret = removexattr(path, key);
        if (ret == -1) {
                if (errno == ENOATTR) {
                        /* not a error, do not print error message */
                        return 0;
                }

                /* using thead safe version of strerror() */
                if (strerror_r(errno, err_buf, ERR_MSG_BUF_LEN) == -1) {
                        err_buf[0] = '\0'; /* very unlikely */
                }

                LOG(DEBUG, "failed to remove extended attribute for %s [key: %s]; reason: %s",
                    path, key, err_buf);

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
        unsigned char location = 0; /* an actual value will be assigned in get_xattr() */

        int ret = get_xattr(path, xattr_str[location], (char *)&location, &size);
        if (ret == -1) {
                return -1; /* something bad happen; report error */
        }

        return ((location == LOCAL_STORE) || (size == 0));
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
                        LOG(ERROR, "failed to get front element of queue");
                        return -1;
                }

                /* save path to local variable to release memory in queue */
                char buf[it_sz];
                memcpy(buf, path, it_sz);
                buf[it_sz - 1] = '\0';    /* ensure that buffer is terminated by \0 character */

                if (queue_pop(queue) == -1) {
                        LOG(ERROR, "unable to pop element from queue");
                        return -1;
                }

                if (!is_valid_path(buf)) {
                        LOG(ERROR, "path %s is not valid", buf);
                        return -1;
                }

                if (is_file_local(buf)) {
                        /* local files should be moved out */
                        if (getconf()->ops.move_file_out(buf) == -1) {
                                LOG(ERROR, "unable to move out file %s", buf);
                                return -1;
                        }
                } else {
                        /* remote files should be moved in */
                        if (getconf()->ops.move_file_in(buf) == -1) {
                                LOG(ERROR, "unable to move in file %s", buf);
                                return -1;
                        }
                }
        }

        /* return successfully if there are no files to move or move in/out operation was successful */
        return 0;
}


/**************************
 * S3 Protocol Operations *
 **************************/
static S3BucketContext bucketContext;

static S3Status
responsePropertiesCallback(const S3ResponseProperties *properties, void *callbackData) {
        return S3StatusOK;
}

static void
responseCompleteCallback(S3Status status, const S3ErrorDetails *error, void *callbackData) {
        return;
}

static S3ResponseHandler responseHandler = {
        .propertiesCallback = &responsePropertiesCallback,
        .completeCallback = &responseCompleteCallback
};

static void s3_init_data(S3BucketContext *bc, S3Protocol tp) {
        conf_t *conf = getconf();

        bc->hostName        = conf->s3_default_hostname;
        bc->bucketName      = conf->s3_bucket;
        bc->protocol        = tp;
        bc->uriStyle        = S3UriStylePath;
        bc->accessKeyId     = conf->s3_access_key_id;
        bc->secretAccessKey = conf->s3_secret_access_key;
        bc->securityToken   = NULL;
}

int s3_init_remote_store_access(void) {
        conf_t *conf = getconf();

        S3Status status = S3_initialize(NULL, S3_INIT_ALL, conf->s3_default_hostname);

        if (status == S3StatusOK) {
                return 0;
        }

        if (status == S3StatusUriTooLong) {
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

        S3Protocol transfer_protocol = strcmp(conf->transfer_protocol, "http") == 0 ?
                                               S3ProtocolHTTP : S3ProtocolHTTPS;
        s3_init_data(&bucketContext, transfer_protocol);

        S3_create_bucket(transfer_protocol,
                         conf->s3_access_key_id,
                         conf->s3_secret_access_key,
                         NULL,
                         conf->s3_default_hostname,
                         conf->s3_bucket,
                         S3CannedAclPrivate,
                         NULL,
                         NULL,
                         &responseHandler,
                         NULL);

        return 0;
}

int s3_move_file_out(const char *path) {
        /* TODO: implement this method */
        return -1;
}

int s3_move_file_in(const char *path) {
        /* TODO: implement this method */
        return -1;
}

void s3_term_remote_store_access(void) {
        S3_deinitialize();
}

static S3Status test_bucket_status;
static char test_bucket_error_details[4096] = { 0 };

static S3Status test_bucket_properties_callback(const S3ResponseProperties *properties, void *callbackData) {
    return S3StatusOK;
}

static void test_bucket_complete_callback(S3Status status, const S3ErrorDetails *error, void *callbackData) {
    (void) callbackData;

    test_bucket_status = status;
    // Compose the error details message now, although we might not use it.
    // Can't just save a pointer to [error] since it's not guaranteed to last
    // beyond this callback
    int len = 0;
    if (error && error->message) {
        len += snprintf(&(test_bucket_error_details[len]), sizeof(test_bucket_error_details) - len,
                        "  Message: %s\n", error->message);
    }
    if (error && error->resource) {
        len += snprintf(&(test_bucket_error_details[len]), sizeof(test_bucket_error_details) - len,
                        "  Resource: %s\n", error->resource);
    }
    if (error && error->furtherDetails) {
        len += snprintf(&(test_bucket_error_details[len]), sizeof(test_bucket_error_details) - len,
                        "  Further Details: %s\n", error->furtherDetails);
    }
    if (error && error->extraDetailsCount) {
        len += snprintf(&(test_bucket_error_details[len]), sizeof(test_bucket_error_details) - len,
                        "%s", "  Extra Details:\n");
        int i;
        for (i = 0; i < error->extraDetailsCount; i++) {
            len += snprintf(&(test_bucket_error_details[len]),
                            sizeof(test_bucket_error_details) - len, "    %s: %s\n",
                            error->extraDetails[i].name,
                            error->extraDetails[i].value);
        }
    }
}


/**
 * Following function was initially copied from s3.c located in
 * https://github.com/bji/libs3/blob/master/src/s3.c and then modified.
 */
static void test_bucket(int argc, char **argv, int optindex)
{
    const char *bucketName = getconf()->s3_bucket;

    S3ResponseHandler response_handler = { &test_bucket_properties_callback,
                                           &test_bucket_complete_callback };

    char location_constraint[64];
    int retries = 5;
    do {
        S3_test_bucket(S3ProtocolHTTPS, S3UriStylePath,
                       getconf()->s3_access_key_id, getconf()->s3_secret_access_key, 0,
                       0, bucketName, sizeof(location_constraint),
                       location_constraint, 0, &response_handler, 0);
        --retries;
    } while (S3_status_is_retryable(test_bucket_status) && retries > 0);

    const char *result;

    switch (test_bucket_status) {
    case S3StatusOK:
        // bucket exists
        result = location_constraint[0] ? location_constraint : "USA";
        break;
    case S3StatusErrorNoSuchBucket:
        result = "Does Not Exist";
        break;
    case S3StatusErrorAccessDenied:
        result = "Access Denied";
        break;
    default:
        result = 0;
        break;
    }

    if (result) {
        LOG(DEBUG, "%-56s  %-20s\n%s%s\n%-56s  %-20s\n",
            "                         Bucket",
            "       Status",
            "--------------------------------------------------------  ",
            "--------------------",
            bucketName, result);
    }
    else {
        LOG(ERROR, "error: %s", S3_get_status_name(test_bucket_status));
    }
}
