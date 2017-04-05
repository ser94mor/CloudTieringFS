/**
 * Copyright (C) 2016, 2017  Sergey Morozov <ser94mor@gmail.com>
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
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <attr/xattr.h>
#include <libs3.h>

#include "ops.h"
#include "conf.h"
#include "log.h"

/* +1 is for '\0' character */
#define S3_XATTR_KEY     "s3_object_id"
#define S3_XATTR_SIZE    S3_MAX_KEY_SIZE + 1

/* buffer to store error messages (mostly errno messages) */
static __thread char err_buf[ERR_MSG_BUF_LEN];

/* buffer to store file's s3 identifiers */
static __thread char s3_xattr_buf[S3_XATTR_SIZE];

/*  context for working with objects within a bucket; initialized only once */
static S3BucketContext g_bucket_context;

/* enum of s3 operation types; used to determine callback behaviour */
enum s3_cb_enum {
        e_s3_cb_test_bucket,
        e_s3_cb_create_bucket,
        e_s3_cb_put_object,
        e_s3_cb_get_object,
};

/* used as in and out a parameter for s3_response_complete_callback() */
struct s3_cb_data {
        enum s3_cb_enum type; /* method identifier */
        S3Status status; /* request status */
        char error_details[4096];
        void *data; /* some data */
};


/* used in s3_put_object_data_callback();
   convinient to give file's stuff as a single argument */
struct s3_put_object_callback_data {
        FILE *file;
        uint64_t content_length;
};

/* used in s3_get_object_data_callback(); not needed since it has only one
   member but still defined in order to be consistent with
   struct s3_put_object_callback_data */
struct s3_get_object_callback_data {
        FILE *file;
};

/**
 * @brief s3_response_properties_callback This callback is made whenever the
 *                                        response properties become available
 *                                        for any request.
 *
 * @param[in]     properties    The properties that are available from the
 *                              response.
 * @param[in,out] callback_data The callback data as specified when the request
 *                              was issued.
 * @return S3StatusOK to continue processing the request, anything else to
 *         immediately abort the request with a status which will be
 *         passed to the S3ResponseCompleteCallback for this request.
 *         Typically, this will return either S3StatusOK or
 *         S3StatusAbortedByCallback.
 */
static S3Status s3_response_properties_callback(
        const S3ResponseProperties *properties, void *callback_data) {
        return S3StatusOK;
}

/**
 * @brief s3_response_complete_callback This callback is made when the response
 *                                      has been completely received, or an
 *                                      error has occurred which has prematurely
 *                                      aborted the request, or one of the
 *                                      other user-supplied callbacks returned
 *                                      a value intended to abort the request.
 *                                      This callback is always made for every
 *                                      request, as the very last callback made
 *                                      for that request.
 *
 * @param[in]     status        Gives the overall status of the response,
 *                              indicating success or failure; use
 *                              S3_status_is_retryable() as a simple way to
 *                              detect whether or not the status indicates that
 *                              the request failed but may be retried.
 * @param[in]     error_details If non-NULL, gives details as returned by the S3
 *                              service, describing the error.
 * @param[in,out] callback_data The callback data as specified when the request
 *                              was issued.
 */
static void s3_response_complete_callback(
        S3Status status,
        const S3ErrorDetails *error_details,
        void *callback_data) {

        struct s3_cb_data *cb_d = (struct s3_cb_data *)callback_data;
        cb_d->status = status;

        int len = 0;
        if (error_details && error_details->message) {
                len += snprintf(&(cb_d->error_details[len]),
                                sizeof(cb_d->error_details) - len,
                                "  Message: %s\n",
                                error_details->message);
        }
        if (error_details && error_details->resource) {
                len += snprintf(&(cb_d->error_details[len]),
                                sizeof(cb_d->error_details) - len,
                                "  Resource: %s\n",
                                error_details->resource);
        }
        if (error_details && error_details->furtherDetails) {
                len += snprintf(&(cb_d->error_details[len]),
                                sizeof(cb_d->error_details) - len,
                                "  Further Details: %s\n",
                                error_details->furtherDetails);
        }
        if (error_details && error_details->extraDetailsCount) {
                len += snprintf(&(cb_d->error_details[len]),
                                sizeof(cb_d->error_details) - len,
                                "%s",
                                "  Extra Details:\n");
                int i;
                for (i = 0; i < error_details->extraDetailsCount; i++) {
                        len += snprintf(&(cb_d->error_details[len]),
                                        sizeof(cb_d->error_details) - len,
                                        "    %s: %s\n",
                                        error_details->extraDetails[i].name,
                                        error_details->extraDetails[i].value);
                }
        }
}

/* defines the callbacks which are made for any request */
static S3ResponseHandler g_response_handler = {
        .propertiesCallback = &s3_response_properties_callback,
        .completeCallback   = &s3_response_complete_callback
};

/**
 * @brief s3_init_globals Initialize global variables related to
 *                        s3 remote storage.
 */
static void s3_init_globals(void) {
        conf_t *conf = get_conf();

        S3Protocol transfer_protocol = strcmp(conf->transfer_protocol,
                                              "http") == 0 ?
                                       S3ProtocolHTTP : S3ProtocolHTTPS;

        g_bucket_context.hostName        = conf->s3_default_hostname;
        g_bucket_context.bucketName      = conf->s3_bucket;
        g_bucket_context.protocol        = transfer_protocol;
        g_bucket_context.uriStyle        = S3UriStylePath;
        g_bucket_context.accessKeyId     = conf->s3_access_key_id;
        g_bucket_context.secretAccessKey = conf->s3_secret_access_key;
        g_bucket_context.securityToken   = NULL;
}

/**
 * @brief s3_bucket_exists Checks an existance of bucket specified
 *                         in configuration.
 *
 * @return  0: bucket not exist or error happen
 *          1: bucket exists
 */
static int s3_bucket_exists(void) {
        /* create bucket if it does not already exist */
        int retries = get_conf()->s3_operation_retries;

        /* prepare callback data structure */
        struct s3_cb_data callback_data = {
                .type = e_s3_cb_test_bucket,
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
        return !!(callback_data.status == S3StatusOK);
}

/**
 * @brief s3_create_bucket Creates bucket in a s3 remote storage.
 *
 * @return  0: bucket has been successfully created
 *         -1: bucket already exists or error happen
 */
static int s3_create_bucket(void) {
        /* create bucket if it does not already exist */
        int retries = get_conf()->s3_operation_retries;

        struct s3_cb_data callback_data = {
                .type = e_s3_cb_create_bucket,
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

/**
 * @brief s3_put_object_data_callback This callback is made during a put
 *                                    object operation, to obtain the next
 *                                    chunk of data to put to the S3 service as
 *                                    the contents of the object. This callback
 *                                    is made repeatedly, each time acquiring
 *                                    the next chunk of data to write to the
 *                                    service, until a negative or 0 value is
 *                                    returned.
 *
 * @param[in]     bufferSize    Gives the maximum number of bytes that may be
 *                              written into the buffer parameter by this
 *                              callback.
 * @param[in,out] buffer        Gives the buffer to fill with at most bufferSize
 *                              bytes of data as the next chunk of data to send
 *                              to S3 as the contents of this object.
 * @param[in,out] callback_data The callback data as specified when the request
 *                              was issued.
 * @return < 0 to abort the request with the S3StatusAbortedByCallback, which
 *        will be pased to the response complete callback for this request, or
 *        0 to indicate the end of data, or > 0 to identify the number of
 *        bytes that were written into the buffer by this callback
 */
static int s3_put_object_data_callback(
        int buffer_size, char *buffer, void *callback_data) {
        struct s3_put_object_callback_data *data =
                (struct s3_put_object_callback_data *)
                                   (((struct s3_cb_data *)callback_data)->data);

        int ret = 0;

        if (data->content_length) {
                int to_read = ((data->content_length > (unsigned) buffer_size) ?
                (unsigned)buffer_size : data->content_length);
                ret = fread(buffer, 1, to_read, data->file);
        }
        data->content_length -= ret;

        return ret;
}

static S3Status s3_get_object_data_callback(
        int buffer_size, const char *buffer, void *callback_data) {
        struct s3_get_object_callback_data *data =
                (struct s3_get_object_callback_data *)
                                   (((struct s3_cb_data *)callback_data)->data);

        size_t wrote = fwrite(buffer, 1, buffer_size, data->file);
        return ((wrote < (size_t)buffer_size) ? S3StatusAbortedByCallback :
                                                S3StatusOK);
}

/**
 * @brief s3_upload Uploads file's data to s3 remote storage.
 *
 * @param[in] path      Path to file which data is going to be uploaded.
 * @param[in] object_id Object id of this file in the remote object storage.
 *
 * @return  0: file's data has been successfully uploaded to s3 remote storage
 *         -1: error happen during process of upload of file's data
 */
int s3_upload( int fd, const char *object_id ) {
        int retries = get_conf()->s3_operation_retries;
        int ret = 0; /* success by default */
        struct s3_put_object_callback_data put_object_data;

        /* set call back data type */
        struct s3_cb_data callback_data = {
                .type = e_s3_cb_put_object,
                .error_details = { 0 },
                .data = &put_object_data,
        };

        /* duplicate file descriptor to be able to close file stream */
        int dup_fd = dup( fd );
        if ( dup_fd == -1 ) {
                /* use thead safe version of strerror() */
                if ( strerror_r( errno, err_buf, ERR_MSG_BUF_LEN ) == -1 ) {
                        err_buf[0] = '\0'; /* very unlikely */
                }

                LOG( ERROR,
                     "[s3_upload] failed to dup() [ fd: %d | reason: %s ]",
                     fd,
                     err_buf );

                return -1;
        }

        /* stat structure for target file to get content length */
        struct stat statbuf;
        if ( fstat( dup_fd , &statbuf ) == -1) {
                /* use thead safe version of strerror() */
                if ( strerror_r( errno, err_buf, ERR_MSG_BUF_LEN ) == -1 ) {
                        err_buf[0] = '\0'; /* very unlikely */
                }

                LOG( ERROR,
                     "[s3_upload] failed to fstat() file "
                     "[ fd: %d | reason: %s ]",
                     dup_fd,
                     err_buf );

                return -1;
        }

        /* initialize structure used during  */
        put_object_data.content_length = statbuf.st_size;
        if ( ! ( put_object_data.file = fdopen( dup_fd, "r" ) ) ) {
                /* use thead safe version of strerror() */
                if ( strerror_r( errno, err_buf, ERR_MSG_BUF_LEN ) == -1 ) {
                        err_buf[0] = '\0'; /* very unlikely */
                }

                LOG( ERROR,
                     "[s3_upload] failed to fdopen file in \"r\" mode "
                     "[ fd: %d | reason: %s ]",
                     dup_fd,
                     err_buf );

                return -1;
        }

        S3PutObjectHandler put_object_handler = {
                .responseHandler = g_response_handler,
                .putObjectDataCallback = &s3_put_object_data_callback,
        };

        do {
                S3_put_object( &g_bucket_context,
                               object_id,
                               put_object_data.content_length,
                               NULL,
                               NULL,
                               &put_object_handler,
                               &callback_data );
        } while( S3_status_is_retryable( callback_data.status ) && --retries );

        /* fail on any error */
        if ( callback_data.status != S3StatusOK ) {
                LOG( ERROR,
                     "[s3_upload] S3_put_object() failed [ error: %s ]",
                     S3_get_status_name( callback_data.status ) );
                LOG( ERROR, callback_data.error_details );
                ret = -1;
        }

        if ( fclose( put_object_data.file ) ) {
                /* this is usually caused by very serious system error */

                /* use thead safe version of strerror() */
                if ( strerror_r( errno, err_buf, ERR_MSG_BUF_LEN ) == -1 ) {
                        err_buf[0] = '\0'; /* very unlikely */
                }

                LOG( ERROR,
                     "[s3_upload] failed to fclose file [fd: %d]",
                     dup_fd,
                     err_buf );
                ret = -1;
        }

        return ret;
}

/**
 * @brief s3_download Downloads file's data from s3 remote storage
 *                    to local storage.
 *
 * @param[in] fd File descriptor of file whose data should be downloaded.
 *
 * @return  0: file's data has been successfully downloaded
 *         -1: error happen during file's data download
 */
int s3_download( int fd, const char *object_id ) {
        int retries = get_conf()->s3_operation_retries;
        int ret = 0; /* success by default */
        struct s3_get_object_callback_data get_object_data;

        /* set call back data type */
        struct s3_cb_data callback_data = {
                .type = e_s3_cb_get_object,
                .error_details = { 0 },
                .data = &get_object_data,
        };

        /* duplicate file descriptor to be able to close file stream */
        int dup_fd = dup( fd );
        if ( dup_fd == -1 ) {
                /* use thead safe version of strerror() */
                if ( strerror_r( errno, err_buf, ERR_MSG_BUF_LEN ) == -1 ) {
                        err_buf[0] = '\0'; /* very unlikely */
                }

                LOG( ERROR,
                     "[s3_download] failed to dup(%d) [reason: %s]",
                     fd,
                     err_buf );

                return -1;
        }

        if ( ( get_object_data.file = fdopen( dup_fd, "w" ) ) == NULL ) {
                /* use thead safe version of strerror() */
                if ( strerror_r( errno, err_buf, ERR_MSG_BUF_LEN ) == -1 ) {
                        err_buf[0] = '\0'; /* very unlikely */
                }

                LOG( ERROR,
                     "[s3_download] failed to fdopen(%d, \"w\") [reason: %s]",
                     dup_fd,
                     err_buf );

                return -1;
        }

        S3GetObjectHandler get_object_handler = {
                g_response_handler,
                &s3_get_object_data_callback
        };

        do {
                S3_get_object( &g_bucket_context,
                               object_id,
                               NULL,
                               0,
                               0,
                               NULL,
                               &get_object_handler,
                               &callback_data );
        } while( S3_status_is_retryable( callback_data.status ) && --retries );

        /* fail on any error */
        if ( callback_data.status != S3StatusOK ) {
                LOG( ERROR,
                     "[s3_download] S3_get_object() failed [error: %s]",
                     S3_get_status_name( callback_data.status ) );
                LOG( ERROR, callback_data.error_details );

                ret = -1;
        }

        if ( fclose( get_object_data.file ) ) {
                /* this is usually caused by very serious system error */

                /* use thead safe version of strerror() */
                if ( strerror_r( errno, err_buf, ERR_MSG_BUF_LEN ) == -1 ) {
                        err_buf[0] = '\0'; /* very unlikely */
                }

                LOG( ERROR,
                     "[s3_download] failed to fclose() [fd: %d; reason: %s]",
                     dup_fd,
                     err_buf );

                ret = -1;
        }

        return ret;
}

/**
 * @brief s3_connect Setups connection with s3 remote storage.
 *
 * @return  0: connection has been successfully setup
 *         -1: error happen during connection setup
 */
int s3_connect(void) {
        conf_t *conf = get_conf();

        S3Status status = S3_initialize(NULL,
                                        S3_INIT_ALL,
                                        conf->s3_default_hostname);


        if (status == S3StatusOK) {
                LOG(DEBUG, "S3_initialize() executed successfully");
        } else if (status == S3StatusUriTooLong) {
                LOG(ERROR,
                    "default s3 hostname %s is longer than %d [error: %s]",
                    conf->s3_default_hostname,
                    S3_MAX_HOSTNAME_SIZE,
                    S3_get_status_name(status));
                return -1;
        } else if (status == S3StatusInternalError) {
                LOG(ERROR,
                    "libs3 dependent libraries could not be initialized "
                    "[error: %s]",
                    S3_get_status_name(status));
                return -1;
        } else if (status == S3StatusOutOfMemory) {
                LOG(ERROR,
                    "out of memory; unable to initialize libs3 [error: %s]",
                    S3_get_status_name(status));
                return -1;
        } else {
                LOG(ERROR,
                    "unknown libs3 error [error: %d]",
                    S3_get_status_name(status));
                return -1;
        }

        /* validates bucket name, not its availability */
        status = S3_validate_bucket_name(conf->s3_bucket, S3UriStylePath);
        if (status != S3StatusOK) {
                LOG(ERROR,
                    "not valid bucket name for path bucket style [error: %s]",
                    S3_get_status_name(status));
                return -1;
        }

        s3_init_globals();

        /* create bucket if does not exist, if already exists do nothing */
        if (!s3_bucket_exists()) {
                if (s3_create_bucket() == -1) {
                        LOG(ERROR,
                            "failed to create bucket %s",
                            conf->s3_bucket);
                        return -1;
                }
                LOG(DEBUG, "bucket %s created successfully", conf->s3_bucket);
        }

        LOG(INFO, "access to remote store setup successfully");

        return 0;
}

/**
 * @brief s3_disconnect Gracefully disconnects from s3 remote storage.
 */
void s3_disconnect(void) {
        S3_deinitialize();
}

char *s3_get_xattr_value(const char *path) {
        /* TODO: implement this function later using near-perfect hash function
                 to compress long paths to keys no more then
                 S3_MAX_KEY_SIZE in length */

        size_t path_len = strlen(path);
        size_t key_len  = ( path_len > S3_MAX_KEY_SIZE ) ?
                          S3_MAX_KEY_SIZE : path_len;

        size_t pos = 0;
        for (; pos < key_len; pos++) {
                char c = path[key_len - pos - 1];
                s3_xattr_buf[pos] = ( c == '/' ) ? '-' : c;
        }
        s3_xattr_buf[pos] = '\0';

        return s3_xattr_buf;
}

size_t s3_get_xattr_size(void) {
        return S3_XATTR_SIZE;
}
