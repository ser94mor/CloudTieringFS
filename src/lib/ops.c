#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <attr/xattr.h>
#include <sys/stat.h>
#include <libs3.h>

#include "cloudtiering.h"

#define XATTR_NAMESPACE    "trusted"
#define XATTR_OBJECT_ID    "object_id"

struct {
        const char *key;
        size_t      size;
} XATTR_SIZE[] = {
        { XATTR_NAMESPACE "." XATTR_OBJECT_ID, S3_MAX_KEY_SIZE }
};


static int is_file_local(const char *path) {
        return -1;
}

static int move_file_out(const char *path) {
        return -1;
}

static int move_file_in(const char *path) {
        return -1;
}

static int is_valid_path(const char *path) {
        struct stat path_stat;
        if ((stat(path, &path_stat) == -1) || !S_ISREG(path_stat.st_mode)) {
                return 0; /* false */
        }

        return 1; /* true */
}

int set_xattr(const char *path, const char *key, const char *value) {
        /* create extended attribute or replace existing */
        int ret = setxattr(path, key, value, strlen(value) + 1, 0);
        if (ret == -1) {
                LOG(ERROR, "failed to set extended attribute [key: %s; value: %s]; reason: %s",
                    key, value, strerror(errno));
                return -1;
        }

        return 0;
}

const char *get_xattr(const char *path, const char *key) {

        int ret = getxattr(path, key, )
        return NULL;
}

int has_xattr(const char *path, const char *key) {
        return 0;
}

/**
 * @brief move_file Move file on the front of the queue in or out (obvious from the file attributes).
 * @return -1 on failure; 0 on success
 */
int move_file(queue_t *queue) {
        if (!queue_empty(queue)) {
                /* get front element in the queue */
                size_t it_sz = 0;
                char *path = queue_front(queue, &it_sz);
                if (path == NULL) {
                        LOG(ERROR, "failed to get front element of queue");
                        return -1;
                }

                /* save path to local variable to release memory in queue */
                char buf[it_sz];
                memcpy(buf, path, it_sz);
                buf[it_sz - 1] = '\0';    /* ensure that buffer is terminated by \0 character */

                if (queue_pop(queue) == -1) {
                        LOG(ERROR, "unable to pop element from queue");
                        return -1;
                }

                if (!is_valid_path(buf)) {
                        LOG(ERROR, "path %s is not valid", buf);
                        return -1;
                }

                if (is_file_local(buf)) {
                        /* local files should be moved out */
                        if (move_file_out(buf) == -1) {
                                LOG(ERROR, "unable to move out file %s", buf);
                                return -1;
                        }
                } else {
                        /* remote files should be moved in */
                        if (move_file_in(buf) == -1) {
                                LOG(ERROR, "unable to move in file %s", buf);
                                return -1;
                        }
                }
        }

        /* return successfully if there are no files to move or move in/out operation was successful */
        return 0;
}
