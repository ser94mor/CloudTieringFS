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

#include <stdio.h>
#include <utime.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

typedef struct {
    int (*open)(const char *path, int flags, ...);                              // need to download file
    int (*open64)(const char *path, int flags, ...);                            // need to download file
    int (*openat)(int dirfd, const char *path, int flags, ...);                 // need to download file
    int (*openat64)(int dirfd, const char *path, int flags, ...);               // need to download file
    int (*truncate)(const char *path, off_t length);                            // need to download file or remove extended attributes if truncated to 0
    int (*truncate64)(const char *path, off64_t length);                        // need to download file or remove extended attributes if truncated to 0
    int (*stat)(const char *path, struct stat *buf);                            // need to override some structure members related to file'data
    int (*stat64)(const char *path, struct stat64 *buf);                        // need to override some structure members related to file'data
    int (*fstatat)(int fd, const char *path, struct stat *buf, int flag);       // need to override some structure members related to file'data
    int (*fstatat64)(int fd, const char *path, struct stat64 *buf, int flag);   // need to override some structure members related to file'data
    int (*futimesat)(int dirfd, const char *path, const struct timeval times[2]); // may need to use it in daemon to leave modification time untouched
    int (*utimes)(const char *path, const struct timeval times[2]);             // may need to use it in daemon to leave modification time untouched
    int (*utime)(const char *path, const struct utimbuf *buf);                  // may need to use it in daemon to leave modification time untouched
    int (*fadvise)(int fd, off_t offset, off_t len, int advice);                // can be used to notify about an intention to open file (we can do early download)
    int (*fadvise64)(int fd, off64_t offset, off64_t len, int advice);          // can be used to notify about an intention to open file (we can do early download)

    FILE *(*fopen)(const char *path, const char *mode);                         // need to download file
    FILE *(*freopen)(const char *path, const char *mode, FILE *stream);         // need to download file
} symbols_t;

#endif /* CLOUDTIERING_SYMS_H */
