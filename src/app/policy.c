#define _XOPEN_SOURCE      500        /* needed to use nftw() */
#define _POSIX_C_SOURCE    200112L    /* required for rlimit */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ftw.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "cloudtiering.h"

/*******************
 * Scan filesystem *
 * *****************/

static queue_t *in_queue  = NULL;
static queue_t *out_queue = NULL;

static int update_evict_queue(const char *fpath, const struct stat *sb,  int typeflag, struct FTW *ftwbuf) {

        struct stat path_stat;
        if (stat(fpath, &path_stat) == -1) {
                /* just continue with the next files; non-zero will
                   cause failure of nftw() */
                return 0;
        }

        if (is_valid_path(fpath) &&
            is_file_local(fpath) &&
            (path_stat.st_atime + 600) < time(NULL)) {

                char *data = (char *)fpath;
                size_t data_size = strlen(fpath) + 1;

                if (queue_push(out_queue, data, data_size) == -1) {
                        LOG(ERROR,
                            "queue_push failed [data: %s; data size: %zu, "
                            "path size max: %zu]",
                            data,
                            data_size,
                            get_conf()->path_max);
                        /* say that error happen, but do not abort execution */
                }
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
