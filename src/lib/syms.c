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

#define _LARGEFILE64_SOURCE    /* needed for off64_t type */
#define _GNU_SOURCE            /* needed for RTLD_NEXT */

#include <dlfcn.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>          /* defines O_* constants */
#include <attr/xattr.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

#include "defs.h"
#include "syms.h"
#include "queue.h"

/* enum of supported extended attributes */
enum xattr_enum {
        XATTRS(ENUMERIZE, COMMA),
};

/* declare array of extended attributes' keys */
static const char *xattr_str[] = {
        XATTRS(XATTR_KEY, COMMA),
};

/* pointer to the first priority download queue in shared memory */
static queue_t *queue = NULL;

/* functions for which this library has wrappers */
static symbols_t symbols = { 0 };

static pthread_once_t once_control = PTHREAD_ONCE_INIT;

/* the pid of this process; will be initialized later once */
static pid_t pid = -1;

symbols_t *get_syms( void ) {
        return &symbols;
}

/* we use extended attribute to determite file's location; we use this
   predicate several times in is_local_file function, so it make sence to
   define it as macros */
#define IS_LOCAL_PREDICATE    ( errno == ENOATTR )    \
                              || ( errno == ENOTSUP ) \
                              || ( errno == ERANGE )


/* This function creates a table of pointers to glibc functions for all
 * of the io system calls so they can be called when needed
 */
void __attribute__ ((constructor)) init_syms(void) {
        /* we do not check the return result of dlsym() here because we do
           do not know what functions from this structure are actually needed
           by the program that uses this library; check for return result will
           be performed in the places of actual invocation */
        symbols.open       = dlsym( RTLD_NEXT, "open"     );
        symbols.openat     = dlsym( RTLD_NEXT, "openat"   );

        symbols.truncate   = dlsym( RTLD_NEXT, "truncate" );

        symbols.fopen      = dlsym( RTLD_NEXT, "fopen"    );
        symbols.freopen    = dlsym( RTLD_NEXT, "freopen"  );
}


/**
 * @brief is_local_file Check a location of file by file descriptor
 *                             (local or remote).
 *
 * @note Operation is atomic according to
 *       http://man7.org/linux/man-pages/man7/xattr.7.html.
 *
 * @param[in] fd    File descriptor to check location.
 * @param[in] flags Flags with which file descriptor was opened.
 *
 * @return  1: if file is in local storage
 *          0: if file is in remote storage
 *         -1: error happen during an attempt to get extended attribute's value
 */
int is_local_file( int fd, int flags ) {
        /* if file was opened in write-only mode, use fsetxattr with
           XATTR_REPLACE which will produce result equivalent to fgetxattr */
        int ret = ( ( flags & O_WRONLY ) == O_WRONLY )
                  ? fsetxattr( fd, xattr_str[e_stub], NULL, 0, XATTR_REPLACE )
                  : fgetxattr( fd, xattr_str[e_stub], NULL, 0 );

        if ( ret == -1 ) {
                if ( IS_LOCAL_PREDICATE ) {
                        /* if ENOTSUP then the file not in our target filesystem
                           and we not manage lifecycle of this file,
                           if ERANGE then e_stub attribute should have
                           a value which is not possible according to then
                           logic of this program and if ENOATTR then the file is
                           definitely local by our convention which means that
                           we can safetly report that file is local */
                        return 1;
                } else if ( errno == EPERM ) {
                        /* only fsetxattr can produce this errno; we are here
                           only in case linux-specific feature inode
                           flags-attributes plays its role; more specifically
                           either FS_APPEND_FL or FS_IMMUTABLE_FL flags
                           are set (see setxattr(2) and ioctl_iflags(2));
                           in many cases the current process can read the file
                           either, so we want to get extended attribute using
                           /proc/self/fd/<fd> file path and, if not works,
                           give up and return error */
                        char path[PROC_SELF_FD_FD_PATH_MAX_LEN];
                        snprintf(path,
                                 PROC_SELF_FD_FD_PATH_MAX_LEN,
                                 PROC_SELF_FD_FD_PATH_TEMPLATE,
                                 (unsigned long long int)fd);
                        if ( getxattr( path, xattr_str[e_stub], NULL, 0) == -1 ) {
                                if ( IS_LOCAL_PREDICATE ) {
                                        /* as before, this conditional branch
                                           indicates that file is local */
                                        return 1;
                                }

                                /* we can do nothing here, return a error
                                   produced by inner stat call */
                                return -1;
                        }

                        /* e_stub atribute is set which means that file
                           is remote */
                        return 0;
                } else {
                        /* errors from stat(2) which should be handled
                           separately in each public libc wrapper function
                           so we return the error here */
                        return -1;
                }
        }

        /* e_stub atribute is set which means that file is remote */
        return 0;
}

/**
 * @brief clear_xattrs Remove all known extended attributes except e_locked.
 *
 * @return  0: all known extended attributed were removed except e_locked
 *         -1: error happen during known extended attributes removal
 *             except e_locked; ENOATTR error is not an error and ignored
 */
int clear_xattrs( int fd ) {
        int ret = 0;
        /* e_locked, if currently set, will be removed by the daemon */

        if (  ( fremovexattr( fd, xattr_str[e_stub] ) == -1 )
           && ( errno != ENOATTR ) ) {
                ret = -1;
        }

        if (  ( fremovexattr( fd, xattr_str[e_object_id] ) == -1 )
           && ( errno != ENOATTR ) ) {
                ret = -1;
        }

        return ret;
}

/**
 * @brief init_vars_once Initialize library's global variables once, i.e.
 *                       using pthread_once().
 */
static void init_vars_once( void ) {
        /* set process' pid */
        pid = getpid();

        /* map shared memory region containing the queue */
        if ( queue == NULL ) {
                int fd = shm_open( QUEUE_SHM_OBJ, O_RDWR, 0 );
                if ( fd == -1 ) {
                        /* initialization result will be checked
                           in the caller */
                        return;
                }

                struct stat sb;
                if ( fstat( fd, &sb ) == -1 ) {
                        /* initialization result will be checked
                           in the caller */
                        return;
                }

                queue = mmap( NULL,
                              sb.st_size,
                              PROT_READ | PROT_WRITE,
                              MAP_SHARED,
                              fd,
                              0 );

                if ( queue == MAP_FAILED ) {
                        queue = NULL;

                        /* initialization result will be checked
                           in the caller */
                        return;
                }
        }
}

/**
 * @brief schedule_download Push file in the first priority download queue.
 *
 * @note Set errno to ENOMEM since this is the only kind of error
 *       within open-calls family that reflects system error.
 *
 * @param[in] fd File descriptor to calculate "proc-path" to be pushed to queue.
 *
 * @return  0: file has been successfully pushed to queue;
 *         -1: error happen during opening of shared memory object containing
 *             queue or queue push operation failed.
 */
int schedule_download( int fd ) {
        /* open shared memory object with queue if not already */
        pthread_once( &once_control, init_vars_once );
        if ( queue == NULL ) {
                /* error happen during mapping of queue from shared memory */
                 errno = ENOMEM;
                return -1;
        }

        char path[PROC_PID_FD_FD_PATH_MAX_LEN];
        snprintf(path,
                 PROC_PID_FD_FD_PATH_MAX_LEN,
                 PROC_PID_FD_FD_PATH_TEMPLATE,
                 (unsigned long long int)pid,
                 (unsigned long long int)fd);

        if ( queue_push( queue, path, PROC_PID_FD_FD_PATH_MAX_LEN ) == -1 ) {
                /* this is very unlikely situation with blocking
                   push operation */
                errno = ENOMEM;
                return -1;
        }

        return 0;
}

/**
 * TODO: completely rewrite and rename; current implementation do many kernel
 *       calls in the loop which is unacceptable; consider using
 *       signal SIGUSR1 or SIGUSR2 as a notification mechanism.
 */
int poll_file_location( int fd, int flags, int should_wait ) {
        /* TODO: completely rewrite; current implementation do many kernel
                 calls in the loop which is unacceptable; consider using
                 signal SIGUSR1 or SIGUSR2 as a notification mechanism */
        int ret = -1;

        do {
                ret = is_local_file( fd, flags );
        } while ( ret == 0 );

        return ( ret == -1 ) ? -1 : 0;
}
