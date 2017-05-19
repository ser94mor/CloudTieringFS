#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <linux/limits.h>

static long long timespec_diff(struct timespec *start, struct timespec *stop) {
        struct timespec result;
    if ((stop->tv_nsec - start->tv_nsec) < 0) {
        result.tv_sec = stop->tv_sec - start->tv_sec - 1;
        result.tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
    } else {
        result.tv_sec = stop->tv_sec - start->tv_sec;
        result.tv_nsec = stop->tv_nsec - start->tv_nsec;
    }

    long long diff_nsec = (result.tv_sec) * 1000000000 + result.tv_nsec;

    return diff_nsec;
}

int main(int argc, char *argv[]) {
        struct timespec before_open_tm;
        struct timespec after_open_tm;
        struct timespec after_read_tm;
        struct timespec diff_tm;

        char log_file[NAME_MAX];
        sprintf(log_file, "perf-test-%s.csv", argv[2]);

        DIR *dir;
        struct dirent *ent;
        if ((dir = opendir (argv[1])) != NULL) {
                FILE *log = fopen(log_file, "w");
                if (log == NULL) {
                        fprintf(stderr,
                                "unable to open log file %s\n",
                                log_file);
                        closedir(dir);
                        return EXIT_FAILURE;
                }

                char log_buf[1024];

                sprintf(log_buf,
                        "SIZE_B,OPEN_DIFF,OPEN_READ_DIFF\n");
                size_t to_write_b = strlen(log_buf);
                size_t ret = fwrite(log_buf, to_write_b, 1, log);
                if (ret != 1) {
                        fprintf(stderr,
                                "error writing file %s\n",
                                log_file);
                        fclose(log);
                        closedir(dir);
                        return EXIT_FAILURE;
                }

                /* print all the files and directories within directory */
                while ((ent = readdir (dir)) != NULL) {
                        if (ent->d_name[0] == '.') {
                                continue;
                        }

                        char path[PATH_MAX];
                        sprintf(path, "%s/%s", argv[1], ent->d_name);

                        if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID,
                                          &before_open_tm)) {
                                fprintf(stderr,
                                        "failed to clock_gettime "
                                        "before open\n");
                                fclose(log);
                                closedir(dir);
                                return EXIT_FAILURE;
                        }


                        int fd = open(path, O_RDONLY);
                        if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID,
                                          &after_open_tm)) {
                                fprintf(stderr,
                                        "failed to clock_gettime "
                                        "after open\n");
                                fclose(log);
                                closedir(dir);
                                return EXIT_FAILURE;
                        }
                        if (fd == -1) {
                                fprintf(stderr,
                                        "failed to open file %s\n",
                                        ent->d_name);
                                fclose(log);
                                closedir(dir);
                                return EXIT_FAILURE;
                        }

                        char *bytes_str = strrchr(ent->d_name, '_');
                        size_t size;
                        if (bytes_str != NULL) {
                                errno=0;
                                size = (size_t) strtoull(bytes_str + 1, NULL, 10);
                                if (errno) {
                                        bytes_str == NULL;
                                }
                        }
                        if (bytes_str == NULL) {
                                fprintf(stderr,
                                        "failed to extract number of bytes "
                                        "from file name %s\n",
                                        ent->d_name);
                                close(fd);
                                fclose(log);
                                closedir(dir);
                                return EXIT_FAILURE;
                        }

                        FILE *file = fdopen(fd, "r");

                        char buf[size];

                        size_t to_read_b = size;
                        size_t ret = fread(buf, size, 1, file);
                        if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID,
                                                &after_read_tm)) {
                                        fprintf(stderr,
                                                "failed to clock_gettime "
                                                "before open\n");
                                        fclose(log);
                                        closedir(dir);
                                        return EXIT_FAILURE;
                        }

                        if (ret != 1) {
                                fprintf(stderr,
                                        "error reading file %s\n",
                                        ent->d_name);
                                fclose(file);
                                fclose(log);
                                closedir(dir);
                                return EXIT_FAILURE;
                        }



                        sprintf(log_buf,
                                "%zu,%lld,%lld\n",
                                size,
                                timespec_diff(&before_open_tm, &after_open_tm),
                                timespec_diff(&before_open_tm, &after_read_tm));
                        size_t to_write_b = strlen(log_buf);
                        ret = fwrite(log_buf, to_write_b, 1, log);
                        if (ret != 1) {
                                fprintf(stderr,
                                        "error writing file %s\n",
                                        ent->d_name);
                                fclose(file);
                                fclose(log);
                                closedir(dir);
                                return EXIT_FAILURE;
                        }

                        fclose(file);
                }

                char end_str = '\0';
                ret = fwrite(&end_str, sizeof(char), 1, log);
                if (ret != 1) {
                        fprintf(stderr,
                                "error reading file %s\n",
                                ent->d_name);
                        fclose(log);
                        closedir(dir);
                        return EXIT_FAILURE;
                }
                closedir (dir);
                fclose(log);
        } else {
                /* could not open directory */
                fprintf(stderr, "failured to open directory %s\n", argv[1]);
                return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
}
