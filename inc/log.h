#ifndef LOG_H
#define LOG_H

/* logger levels */
#define TRACE 0
#define DEBUG 1
#define INFO  2
#define WARN  3
#define ERROR 4
#define FATAL 5

/* current logger level */
#define CUR_LOG_LVL DEBUG

/* main logger function */
#define LOG(level,format,args...) { \
            if (CUR_LOG_LVL <= level) { \
                fprintf(stderr, format, ## args); \
                fprintf(stderr, "\n"); \
                fflush(stderr); \
            } \
        }

#endif // LOG_H
