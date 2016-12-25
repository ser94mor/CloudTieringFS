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

/********************************************************************************
* Code in this file is a proof-of-concept of preloading self-make method fopen. *
* This file will be used as a reference while implementing this feature in      *
* the main code. It will be removed while no longer needed.                     *
********************************************************************************/

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <libs3.h>

#include "cloudtiering.h"

typedef FILE *(*fopen_t)(const char *filename, const char *mode);

static S3Status
s3_response_properties_callback(const S3ResponseProperties *properties, void *callbackData) {
        return S3StatusOK;
}

enum s3_cb_type {
        s3_cb_GET_OBJECT,
};

/* used as in and out a parameter for s3_response_complete_callback() */
struct s3_cb_data {
        enum s3_cb_type type; /* method identifier */
        S3Status status; /* request status */
        char error_details[4096];
        void *data; /* some data */
};


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


static S3Status getObjectDataCallback(int bufferSize, const char *buffer, void *callbackData)
{
        FILE *outfile = (FILE *) (((struct s3_cb_data *)callbackData)->data);

        size_t wrote = fwrite(buffer, 1, bufferSize, outfile);
        return ((wrote < (size_t) bufferSize) ? S3StatusAbortedByCallback : S3StatusOK);
}



/*  context for working with objects within a bucket. Initialized only once. */
static S3BucketContext g_bucket_context;

FILE *fopen(const char *filename, const char *mode) {
        fopen_t c_fopen;
        c_fopen = (fopen_t)dlsym(RTLD_NEXT,"fopen");

        if (!strncmp("/tmp/orangefs", filename, 13)) {
                readconf("conf/cloudtiering-monitor.conf");

                conf_t *conf = getconf();

                if (conf != NULL) {
                        OPEN_LOG("wrapper");

                        g_bucket_context.hostName        = conf->s3_default_hostname;
                        g_bucket_context.bucketName      = conf->s3_bucket;
                        g_bucket_context.protocol        = S3ProtocolHTTPS;
                        g_bucket_context.uriStyle        = S3UriStylePath;
                        g_bucket_context.accessKeyId     = conf->s3_access_key_id;
                        g_bucket_context.secretAccessKey = conf->s3_secret_access_key;
                        g_bucket_context.securityToken   = NULL;

                        int retries = 5;

                        struct s3_cb_data callback_data = {
                                .type = s3_cb_GET_OBJECT,
                        };

                        S3GetObjectHandler getObjectHandler = {
                                g_response_handler,
                                &getObjectDataCallback
                        };

                        FILE *stream = c_fopen(filename, "w");
                        callback_data.data = stream;
                        do {
                                S3_get_object(&g_bucket_context,
                                              filename,
                                              NULL,
                                              0,
                                              0,
                                              NULL,
                                              &getObjectHandler,
                                              &callback_data);
                        } while(S3_status_is_retryable(callback_data.status) && --retries);

                        LOG(DEBUG, "Get Object Status: %s", S3_get_status_name(callback_data.status));

                        fclose(stream);



                }
        }


    return c_fopen(filename, mode);
}
