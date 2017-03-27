#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "defs.h"
#include "syms.h"

int stat( const char *path, struct stat *buf ) {
        /* in case openat call was not initialized in the library constructor
           return ELIBACC; open can not return such error in Linux
           and the caller does not expect it and should fail, since we are
           unable to handle this call */
        if ( ( get_syms()->open == NULL ) || ( get_syms()->fstat == NULL ) ) {
                errno = ELIBACC;
                return -1;
        }

        /* make a call at first to get file descriptor first;
           if failed, just return error and do no more actions;
           use O_RDONLY to perform operations such as fgetxattr */
        int fd = get_syms()->open( path, O_RDONLY );

        if ( fd == -1 ) {
                /* FIXME: errno from open should be properly mapped to stat's */
                return fd;
        }

        int is_local = is_local_file( fd, O_RDONLY );
        if ( is_local == -1 ) {
                /* FIXME: errno from is_local_file should be handled */
                return -1;
        }

        if ( get_syms()->fstat( fd, &buf ) == -1 ) {
                /* TODO: handle errors */

                int err = errno;

                /* we can do nothing about close() failures here */
                close( fd );

                errno = err;

                return -1;
        }

        if ( is_local ) {
               return 0;
        }

        /* we are here when file is remote and we need to get extended attribute from */
}

int lstat( const char *path, struct stat *buf ) {

}

int fstatat( int dir_fd, const char *path, struct stat *buf, int flags ) {

}

