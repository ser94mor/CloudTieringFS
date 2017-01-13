#ifndef POSIX_H
#define POSIX_H

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
} posix_ops_t;

#endif /* POSIX_H */
