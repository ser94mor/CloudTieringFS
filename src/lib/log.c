#include <stdarg.h>
#include <syslog.h>

#include "cloudtiering.h"

typedef struct {
        void (*open_log)(const char *);
        void (*log)(int, const char *, ...);
        void (*close_log)(void);
} log_t;

/*
 * Functions for 'syslog'.
 */
inline void syslog_open_log(const char *name) {
        openlog(name, LOG_PID, LOG_DAEMON);
}

inline void syslog_log(int level, const char *msg_fmt, ...) {
        va_list args;
        va_start(args, msg_fmt);
        
        vsyslog(level, msg_fmt, args);
        
        va_end(args);
}

inline void syslog_close_log(void) {
        closelog();
}


/*
 * Functions for 'default'.
 */
inline void default_open_log(const char *name) {
        /* no action */
}

inline void default_log(int level, const char *msg_fmt, ...) {
        /* no action */
}

inline void default_close_log(void) {
        /* no action */
}
