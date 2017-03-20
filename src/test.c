#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main() {
    char path[20] = "/mnt/orangefs/file0";

    for (int i = 0; i < 10; i++) {
        path[18] = '0' + (char)i;
        int fd = open(path, O_RDONLY);
        char buf[256];
        memset(buf, '\0', 256);
        printf("file: %s\nfd: %d\n", path, fd);

        char *ptr = buf;

        ssize_t cnt = 0;
        do {
            cnt = read(fd, ptr, 255);
            if (cnt == -1) {
                printf("error occured during reading\n");
                return -1;
             }
             ptr += cnt;
        } while (cnt > 0);

        printf("content: %s\n", buf);

        if ( close(fd) == -1 ) {
            return 1;
        }
    }

    return 0;
}
