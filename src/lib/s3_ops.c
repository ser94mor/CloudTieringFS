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

#define _POSIX_C_SOURCE    200112L    /* required for strerror_r() */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <attr/xattr.h>
#include <libs3.h>

#include "cloudtiering.h"

static __thread char err_buf[ERR_MSG_BUF_LEN];

/* declare array of extended attributes' keys */
static const char *xattr_str[] = {
        XATTRS(XATTR_KEY, COMMA),
};

/* declare array of extended attributes' sizes */
static size_t xattr_max_size[] = {
        XATTRS(XATTR_MAX_SIZE, COMMA),
};

/* declare types of extended attributes' values */
XATTRS(DECLARE_XATTR_TYPE, SEMICOLON);

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

int s3_upload(const char *path) {
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

        const char *key = xattr_str[e_s3_object_id];
        xattr_s3_object_id_t value = (xattr_s3_object_id_t)path;

        if (setxattr(path, key, value, xattr_max_size[e_s3_object_id], XATTR_CREATE) == -1) {
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

        return ret;
}

int s3_connect(void) {
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

int s3_download(const char *path) {
        /* TODO: need to implement this function */
        return -1;
}

void s3_disconnect(void) {
        S3_deinitialize();
}
