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

#ifndef CLOUDTIERING_S3_INTERFACE_H
#define CLOUDTIERING_S3_INTERFACE_H

#include <sys/types.h>

/*
 * S3 Protocol Specific Implementation of Function from ops_t.
 */
int    s3_connect( void );
int    s3_download( int fd, const char *object_id );
int    s3_upload( int fd, const char *object_id );
void   s3_disconnect( void );
char  *s3_get_object_id_xattr_value( const char *path );
size_t s3_get_object_id_xattr_size( void );

#endif /* CLOUDTIERING_S3_INTERFACE_H */
