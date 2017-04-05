/**
 * Copyright (C) 2017  Sergey Morozov <ser94mor@gmail.com>
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

#ifndef CLOUDTIERING_CONF_H
#define CLOUDTIERING_CONF_H

/*******************************************************************************
* CONFIGURATION                                                                *
* -------------                                                                *
*                                                                              *
* TODO: write description                                                      *
*******************************************************************************/

#include <stddef.h>
#include <time.h>

#include "defs.h"

/* a list of sections' names in configuration file */
#define SECTIONS(action, sep)     \
        action(General)      sep  \
        action(Internal)     sep  \
        action(S3RemoteStore)

/* a macro-function to declare begining and end tokens for a given section */
#define DECLARE_SECTION_STR(elem)                                      \
        static const char beg_##elem##_section_str[] = "<"  #elem ">", \
                          end_##elem##_section_str[] = "</" #elem ">"

/* a macro-function that produces dotconf's context value
   (it is greater than 0 because dotconf's CTX_ALL section macros is 0) */
#define SECTION_CTX(elem)     ENUMERIZE(elem) + 1

/* enum of supported sections in configuration */
enum section_enum {
        SECTIONS(ENUMERIZE, COMMA),
};

/* TODO: this massive struct is legacy and
         should be devided to smaller units */
typedef struct {
        /* 4096 is minimum acceptable value (including null) according to POSIX (equals PATH_MAX from linux/limits.h) */
        char   fs_mount_point[4096];          /* filesystem's root directory */

        time_t scanfs_iter_tm_sec;            /* the lowest time interval between file system scan iterations */

        double move_out_start_rate;           /* start evicting files when storage is (move_out_start_rate * 100)% full */
        double move_out_stop_rate;            /* stop evicting files when storage is (move_out_stop_rate * 100)% full */

        size_t primary_download_queue_max_size;
        size_t secondary_download_queue_max_size;

        size_t primary_upload_queue_max_size;
        size_t secondary_upload_queue_max_size;

        size_t path_max;                      /* maximum path length in fs_mount_point directory can not be lower than this value */

        /* 63 it is unlikely that protocol identifies require more symbols */
        char   remote_store_protocol[64];

        /* 15 enough to store string id of transfer protocol */
        char transfer_protocol[16];

        /* 255 equals to S3_MAX_HOSTNAME_SIZE from libs3 */
        char   s3_default_hostname[256];      /* s3 default hostname (libs3) */

        /* 63 is maximum allowed bucket name according to http://docs.aws.amazon.com/AmazonS3/latest/dev/BucketRestrictions.html */
        char   s3_bucket[64];                 /* name of s3 bucket which will be used as a remote tier */

        /* 32 is maximum access key id length according to http://docs.aws.amazon.com/IAM/latest/APIReference/API_AccessKey.html */
        char   s3_access_key_id[33];          /* s3 access key id */

        /* 127 is an assumption that in practice longer keys do not exist */
        char   s3_secret_access_key[128];     /* s3 secret access key */

        /* maximum number of retries of s3 requests */
        int s3_operation_retries;
} conf_t;

int read_conf(const char *conf_path);
conf_t *get_conf();

#endif    /* CLOUDTIERING_CONF_H */
