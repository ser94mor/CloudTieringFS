/**
 * Copyright (C) 2017  Sergey Morozov <sergey94morozov@gmail.com>
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


#ifndef CLOUDTIERING_OPS_H
#define CLOUDTIERING_OPS_H

/*******************************************************************************
* OPERATIONS                                                                   *
* ----------                                                                   *
*                                                                              *
* TODO: write description                                                      *
*******************************************************************************/

#include <sys/types.h>

#include "defs.h"

/* a list of supported protocols */
#define PROTOCOLS(action, sep) \
        action(s3)

/* enum of supported protocols */
enum protocol_enum {
        PROTOCOLS(ENUMERIZE, COMMA),
};

/* a macro-function to declare ops_t data-structure for a given protocol */
#define DECLARE_OPS(elem)                                       \
        static ops_t elem##_ops = {                             \
                .protocol        = ENUMERIZE(elem),             \
                .connect         = elem##_connect,              \
                .download        = elem##_download,             \
                .upload          = elem##_upload,               \
                .disconnect      = elem##_disconnect,           \
                .get_object_id_xattr_value = elem##_get_object_id_xattr_value,      \
                .get_object_id_xattr_size  = elem##_get_object_id_xattr_size,       \
        }

typedef struct {
        /* protocol identifier */
        enum protocol_enum protocol;

        /* this function will be called to establish connection
           to remote storage */
        int    (*connect)( void );

        /* this function will be called to perform file download operation */
        int    (*download)( int fd, const char *object_id );

        /* this function will be called to perform file upload operation */
        int    (*upload)( int fd, const char *object_id );

        /* this function will be called to gracefully disconnect from
           the remote storage */
        void   (*disconnect)( void );

        /* get value of an object id xattr for the given path for the specific
           remote storage */
        char  *(*get_object_id_xattr_value)( const char *path );

        /* get maximum size of an object id xattr for the given path for the
           specific remote storage */
        size_t (*get_object_id_xattr_size) ( void );
} ops_t;

ops_t  *get_ops();

/*
 * S3 Protocol Specific Implementation of Function from ops_t.
 */
int    s3_connect( void );
int    s3_download( int fd, const char *object_id );
int    s3_upload( int fd, const char *object_id );
void   s3_disconnect( void );
char  *s3_get_object_id_xattr_value( const char *path );
size_t s3_get_object_id_xattr_size( void );


/**
 * @brief dowload_file Download file from remote storage to local storage.
 *
 * @param[in] path Path to file to download.
 *
 * @return  0: file has been successfully dowloaded
 *         -1: failure happen due dowload of file or
 *             file is currently being downloaded by another thread
 */
int download_file( const char *path );

/**
 * @brief upload_file Upload file to remote storage from local storage.
 *
 * @param[in] path Path to a file to upload.
 *
 * @return  0: file has been upload to remote storage properly and truncated
 *         -1: file has not been upload due to error or access to file
 *             has happen during eviction
 */
int upload_file( const char *path );

#endif    /* CLOUDTIERING_OPS_H */
