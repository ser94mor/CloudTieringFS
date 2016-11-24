#define _XOPEN_SOURCE 500    /* needed to use nftw() */

#include <unistd.h>
#include <string.h>
#include <ftw.h>
#include <sys/stat.h>
#include <sys/resource.h>

#include "cloudtiering.h"

/* queue might be full and we need to perform several attempts giving a chance
 * for other threads to pop elements from queue */
#define QUEUE_PUSH_RETRIES              3
#define QUEUE_PUSH_ATTEMPT_SLEEP_SEC    1

static const queue_t *in_queue  = NULL;
static const queue_t *out_queue = NULL;

static int update_evict_queue(const char *fpath, const struct stat *sb,  int typeflag, struct FTW *ftwbuf) {
        int attempt = 0;

        struct stat path_stat;
        if (stat(fpath, &path_stat) == -1) {
                return 0; /* just continue with the next files; non-zero will cause failure of nftw() */
        }

        if (S_ISREG(path_stat.st_mode)) {
                do {
                        if (queue_push((queue_t *)out_queue, (char *)fpath, strlen(fpath) + 1) == -1) {
                                ++attempt;
                                LOG(DEBUG, "failed to push file name '%s' to out queue (attempt %d/%d)",
                                    fpath, attempt, QUEUE_PUSH_RETRIES);
                                continue;
                        }
                        LOG(DEBUG, "file '%s' pushed to out queue", fpath);
                        break;
                } while ((attempt < QUEUE_PUSH_RETRIES) && (queue_full((queue_t *)out_queue) > 0));
        }

        return 0;
}

int scanfs(queue_t *in_q, queue_t *out_q) {
        conf_t *conf = getconf();

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
