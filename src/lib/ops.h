#ifndef OPS_H
#define OPS_H

#include <cloudtiering.h>

/**
 * S3 PROTOCOL
 */

int  s3_init_remote_store_access(void);
int  s3_move_file_in(const char *path);
int  s3_move_file_out(const char *path);
void s3_term_remote_store_access(void);

#endif /* OPS_H */
