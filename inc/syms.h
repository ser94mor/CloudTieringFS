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

#ifndef CLOUDTIERING_SYMS_H
#define CLOUDTIERING_SYMS_H

#define _GNU_SOURCE        /* needed for *64 symbols */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

/* Since this library aims to be a POSIX-conformant, there is no need to
   implement LFS specification because it is an ptional feature in SUSv2+.
   It is not presented in POSIX */

typedef struct {
    int (*open)( const char *path, int flags, ... );
    int (*openat)(int dirfd, const char *path, int flags, ...);

    int (*truncate)(const char *path, off_t length);

    int (*stat)(const char *path, struct stat *buf);
    int (*fstatat)(int fd, const char *path, struct stat *buf, int flag);


    FILE *(*fopen)(const char *path, const char *mode);                         // need to download file
    FILE *(*freopen)(const char *path, const char *mode, FILE *stream);         // need to download file
} symbols_t;

symbols_t *get_syms( void );

#define IS_LOCAL_FILE( arg ) \
        is_local_file( _Generic( ( arg ), \
                                 int: arg, \
                                 default: -1 ), \
                       _Generic( ( arg ), \
                                 char *: arg, \
                                 default: NULL ) \
                     )

int is_local_file( int fd, const char *path );
int schedule_download( int fd );
int poll_file_location( int fd, const char *path, int should_wait );

#endif /* CLOUDTIERING_SYMS_H */
