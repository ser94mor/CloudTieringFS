#define _POSIX_C_SOURCE    200112L    /* required for strerror_r() */

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <attr/xattr.h>
#include <sys/stat.h>
#include <libs3.h>

#include "cloudtiering.h"

/* macroses calculating maximum among macrosed values */
#define MAX_2(x1, x2)         ( ((x1) > (x2)) ? (x1) : (x2) )
#define MAX_3(x1, x2, x3)     ( ((x1) > (MAX_2(x2,x3))) ? (x1) : (MAX_2(x2,x3)) )
#define MAX_4(x1, x2, x3, x4) ( ((x1) > (MAX_3(x2,x3,x4))) ? (x1) : (MAX_3(x2,x3,x4)) )
#define MAX    MAX_2

/* more than enough to store errno descriptions */
#define ERR_MSG_BUF_LEN    1024

/*
 * Definitions related to extended attributes used to store
 * information about remote storage object location.
 */
#define XATTR_NAMESPACE             "trusted"
#define XATTR_KEY(short_xattr)      XATTR_NAMESPACE "." short_xattr

#define XATTR_OBJECT_ID             XATTR_KEY("object_id")
#define XATTR_OBJECT_ID_MAX_SIZE    S3_MAX_KEY_SIZE    /* defined in libs3.h */

/* maximum possible maximum size of all valid extended attributes */
#define XATTR_MAX_SIZE_MAX          MAX( \
                                        XATTR_OBJECT_ID_MAX_SIZE, \
                                        -1 \
                                    )

static char *attr_list[] = {
       XATTR_OBJECT_ID,
};

static int set_xattr(const char *path, const char *key, const char *value) {
         /* create extended attribute or replace existing */
        int ret = setxattr(path, key, value, strlen(value) + 1, 0);
        if (ret == -1) {
                /* use thead safe version of strerror() */
                char err_buf[ERR_MSG_BUF_LEN];
                if (strerror_r(errno, err_buf, ERR_MSG_BUF_LEN) == -1) {
                        err_buf[0] = '\0'; /* very unlikely */
                }

                LOG(DEBUG, "failed to set extended attribute for %s [key: %s; value: %s]; reason: %s",
                    path, key, value, err_buf);

                return -1;
        }

        return 0;
}

static int get_xattr(const char *path, const char *key, char *value_buf, size_t *size) {
        ssize_t ret = getxattr(path, key, value_buf, *size);
        if (ret == -1) {
                if (errno == ENOATTR) {
                        /* attribute does not exist or process does not have an access to it */
                        *size = 0;
                        return 0;
                }

                /* using thead safe version of strerror() */
                char err_buf[ERR_MSG_BUF_LEN];
                if (strerror_r(errno, err_buf, ERR_MSG_BUF_LEN) == -1) {
                        err_buf[0] = '\0'; /* very unlikely */
                }

                LOG(DEBUG, "failed to get extended attribute for %s [key: %s]; reason: %s",
                    path, key, err_buf);

                return -1;
        }

        return 0;
}

static void remove_xattr(const char *path, const char *key) {
        /* handle all possible errors here */
        int ret = removexattr(path, key);
        if (ret == -1) {
                if (errno == ENOATTR) {
                        /* not a error, do not print error message */
                        return;
                }

                /* using thead safe version of strerror() */
                char err_buf[ERR_MSG_BUF_LEN];
                if (strerror_r(errno, err_buf, ERR_MSG_BUF_LEN) == -1) {
                        err_buf[0] = '\0'; /* very unlikely */
                }

                LOG(DEBUG, "failed to remove extended attribute for %s [key: %s]; reason: %s",
                    path, key, err_buf);
        }
}

static int is_file_local(const char *path) {
        size_t sz = XATTR_MAX_SIZE_MAX;

        /* do not care about return value, care only about sz value */
        for (int i = 0; i < sizeof(attr_list) / sizeof(attr_list[0]); i++) {
                get_xattr(path, attr_list[i], NULL, &sz);
                if (sz != 0) {
                        return 0; /* false; file is not local */
                }
        }

        return 1; /* true; file is local */
}

static int move_file_out(const char *path) {


        return -1;
}

static int move_file_in(const char *path) {
        return -1;
}

static int is_valid_path(const char *path) {
        struct stat path_stat;
        if ((stat(path, &path_stat) == -1) || !S_ISREG(path_stat.st_mode)) {
                return 0; /* false */
        }

        return 1; /* true */
}

/**
 * @brief move_file Move file on the front of the queue in or out (obvious from the file attributes).
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
                        if (move_file_out(buf) == -1) {
                                LOG(ERROR, "unable to move out file %s", buf);
                                return -1;
                        }
                } else {
                        /* remote files should be moved in */
                        if (move_file_in(buf) == -1) {
                                LOG(ERROR, "unable to move in file %s", buf);
                                return -1;
                        }
                }
        }

        /* return successfully if there are no files to move or move in/out operation was successful */
        return 0;
}

int init_remote_store_access() {
        S3Status status = S3_initialize(NULL, S3_INIT_ALL, getconf()->s3_default_hostname);

        if (status == S3StatusOK) {
                return 0;
        }

        if (status == S3StatusUriTooLong) {
                LOG(ERROR, "default s3 hostname %s is longer than %d (error: %s)",
                    getconf()->s3_default_hostname, S3_MAX_HOSTNAME_SIZE, S3_get_status_name(status));
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

        status = S3_validate_bucket_name(getconf()->s3_bucket, S3UriStyleVirtualHost);
        if (status == S3StatusOK) {
                return 0;
        } else {
                LOG(ERROR, "not valid bucket name for virtual host bucket style (error: %s)",
                    S3_get_status_name(status));
                return -1;
        }

        return -1; /* unreachable place */
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
