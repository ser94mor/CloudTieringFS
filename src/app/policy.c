/**
 * Copyright (C) 2016, 2017  Sergey Morozov <sergey@morozov.ch>
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

#define _XOPEN_SOURCE      500        /* needed to use nftw() */
#define _POSIX_C_SOURCE    200112L    /* required for rlimit */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ftw.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "log.h"
#include "ops.h"
#include "conf.h"
#include "queue.h"
#include "file.h"

/*******************
 * Scan filesystem *
 * *****************/

#define EVICTION_TIMEOUT    30

static queue_t *in_queue  = NULL;
static queue_t *out_queue = NULL;

static int update_evict_queue( const char *path,
                               const struct stat *sb,
                               int typeflag,
                               struct FTW *ftwbuf ) {
        /* since we mostly use file descriptors for file operations in other
           places for certain reasons, use file descriptors here as well;
           ignore errors of system calls, we do not want to fail the program
           because of failures in background threads (such as this one) */

        /* cannot use O_PATH flag because some of the next operations will use
           fgetxattr(3)
           (see http://man7.org/linux/man-pages/man2/open.2.html) */
        int fd  = open( path, O_RDWR );
        if ( fd == -1 ) {
                /* just continue with the next files; non-zero will
                   cause failure of nftw() */
                return 0;
        }

        struct stat path_stat;
        if ( fstat( fd, &path_stat ) == -1) {
                /* just continue with the next files; non-zero will
                   cause failure of nftw() */

                if ( close( fd ) == -1 ) {
                        /* TODO: consider to handle EINTR */
                }

                return 0;
        }

        if ( ( is_regular_file( fd ) > 0 )
             && ( is_local_file( fd )   > 0 )
             && ( path_stat.st_atime + EVICTION_TIMEOUT ) < time( NULL ) ) {

                char *data = (char *)path;
                size_t data_size = strlen( path ) + 1;

                if ( queue_push( out_queue, data, data_size ) == -1 ) {
                        LOG( ERROR,
                             "queue_push failed [data: %s; data size: %zu, "
                             "path size max: %zu]",
                             data,
                             data_size,
                             get_conf()->path_max );
                        /* say that error happen, but do not abort execution */
                }
        }

        if ( close( fd ) == -1 ) {
                /* TODO: consider to handle EINTR */
        }

        return 0;
}

int scan_fs(queue_t *in_q, queue_t *out_q) {
        conf_t *conf = get_conf();

        in_queue  = in_q;
        out_queue = out_q;

        /* set maximum number of open files for nftw to a half of descriptor table size for current process */
        struct rlimit rlim;
        if (getrlimit(RLIMIT_NOFILE, &rlim) == -1) {
                return -1;
        }

        /* rlim_cur value will be +1 higher than actual according to documentation */
        if (rlim.rlim_cur <= 2) {
                /* there are 1 or less descriptors available */
                return -1;
        }
        int nopenfd = rlim.rlim_cur / 2;

        /* stay within filesystem and do not follow symlinks */
        int flags = FTW_MOUNT | FTW_PHYS;

        if (nftw(conf->fs_mount_point, update_evict_queue, nopenfd, flags) == -1) {
                return -1;
        }

        return 0;
}
