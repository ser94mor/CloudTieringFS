#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"

static char *item[] = {
                "Hello, World!",
                "This is me.",
                "Let's play a game.",
                "Don't be so shame.",
                "Good luck!",
                "Very very long line that does not fit limits." };

#define QUEUE_MAX_SIZE    3
#define ITEM_MAX_SIZE     20

int test_queue(char *err_msg) {
        queue_t *queue = NULL;

        queue = queue_alloc(QUEUE_MAX_SIZE, ITEM_MAX_SIZE);
        if (queue == NULL) {
                strcpy(err_msg, "'queue_alloc' failed");
                return 1;
        }

        if (!queue_empty(queue)) {
                strcpy(err_msg, "'queue_empty' failed; queue should be empty");
                return 1;
        }

        int i = 0;
        for (i = 0; i < QUEUE_MAX_SIZE; i++) {
                if(queue_push(queue, item[i], strlen(item[i]) + 1)) {
                        strcpy(err_msg, "'queue_push' failed; ordinary case");
                        return 1;
                }

                if (queue_empty(queue)) {
                        strcpy(err_msg, "'queue_empty' failed; queue should not be empty");
                        return 1;
                }
        }

        if (!queue_full(queue)) {
                strcpy(err_msg, "'queue_full' failed; queue should be full");
                return 1;
        }

        ++i; /* increment item index */
        if (!queue_push(queue, item[i], strlen(item[i]))) {
                strcpy(err_msg, "'queue_push' failed; queue should have returned error");
                return 1;
        }

        size_t it_sz = 0;
        char *it = queue_front(queue, &it_sz);

        printf("foo: %uz\n", it_sz);
        if (strcmp(item[0], it) || it_sz != (strlen(item[0]) + 1)) {
                strcpy(err_msg, "'queue_front' failed; wrong return result");
                return 1;
        }

        if (queue_pop(queue)) {
                strcpy(err_msg, "'queue_pop' failed; ordinary case");
                return 1;
        }

        queue_print(stdout, queue);

        return 0;
}
