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
#include <utime.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

/* Since this library aims to be a POSIX-conformant, there is no need to
   implement LFS specification because it is an ptional feature in SUSv2+.
   It is not presented in POSIX */

typedef struct {
    int (*open)( const char *path, int flags, ... );
    int (*openat)(int dirfd, const char *path, int flags, ...);
    int (*truncate)(const char *path, off_t length);                            // need to download file or remove extended attributes if truncated to 0
    int (*stat)(const char *path, struct stat *buf);                            // need to override some structure members related to file'data
    int (*fstatat)(int fd, const char *path, struct stat *buf, int flag);       // need to override some structure members related to file'data


    FILE *(*fopen)(const char *path, const char *mode);                         // need to download file
    FILE *(*freopen)(const char *path, const char *mode, FILE *stream);         // need to download file
} symbols_t;

symbols_t *get_syms( void );

int is_file_local( int fd );
int schedule_download( int fd );
int poll_file_location( int fd, int should_wait );

#endif /* CLOUDTIERING_SYMS_H */
