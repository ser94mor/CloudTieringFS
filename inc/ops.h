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
                .get_xattr_value = elem##_get_xattr_value,      \
                .get_xattr_size  = elem##_get_xattr_size,       \
        }

typedef struct {
        enum protocol_enum protocol;
        int    (*connect) ( void );
        int    (*download)( const char *path, const char *object_id );
        int    (*upload)  ( const char *path, const char *object_id );
        void   (*disconnect)( void );
        char  *(*get_xattr_value)( const char *path );
        size_t (*get_xattr_size) ( void );
} ops_t;

ops_t  *get_ops();

int    s3_connect(void);
int    s3_download(const char *path, const char *object_id);
int    s3_upload(const char *path, const char *object_id);
void   s3_disconnect(void);
char  *s3_get_xattr_value(const char *path);
size_t s3_get_xattr_size(void);

int download_file(const char *path);
int upload_file(const char *path);
int is_file_local(const char *path);
int is_valid_path(const char *path);

#endif    /* CLOUDTIERING_OPS_H */
