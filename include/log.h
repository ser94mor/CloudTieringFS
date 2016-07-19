#ifndef LOG_H
#define LOG_H

/* logger levels */
#define TRACE 0
#define DEBUG 1
#define INFO  2
#define WARN  3
#define ERROR 4

const char *LOG_LVL_STR[] = { "TRACE", "DEBUG", "INFO", "WARN", "ERROR" };

/* current logger level */
#define CUR_LOG_LVL DEBUG

/* main logger function */
#define log(level,format,args...) { \
            if (CUR_LOG_LVL <= level) { \
                fprintf(stderr, format, ## args); \
                fprintf(stderr, "\n"); \
                fflush(stderr); \
            } \
        }

/* wrappers around log(...) function */
#define trace(format, args...) log(TRACE, format, ## args)
#define debug(format, args...) log(DEBUG, format, ## args)
#define info(format, args...)  log(INFO, format, ## args)
#define warn(format, args...)  log(WARN, format, ## args)
#define error(format, args...) log(ERROR, format, ## args)

#endif // LOG_H
