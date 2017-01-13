#ifndef STDIO_H
#define STDIO_H

typedef struct {
    FILE *(*fopen)(const char *path, const char *mode);                         // need to download file
    FILE *(*freopen)(const char *path, const char *mode, FILE *stream);         // need to download file
} stdio_ops_t;

#endif /* STDIO_H */
