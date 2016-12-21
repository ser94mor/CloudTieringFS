#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cloudtiering.h"

static char *item[] = {
                "Hello, World!",
                "This is me.",
                "Let's play a game.",
                "Don't be so shy.",
                "Good luck!",
                "Hello? Hello? C-can you here me? I'm supposed to be too big, to exceed itém limít."
};

#define QUEUE_MAX_SIZE    3
#define ITEM_MAX_SIZE     20

static int queue_print(FILE *stream, queue_t *queue) {
        if (queue == NULL || stream == NULL) {
                return -1;
        }

        pthread_mutex_lock(&queue->mutex);

        char *q_ptr = queue->head;

        char buf[queue->max_item_size + 1];

        /* header */
        fprintf(stream, "QUEUE:\n");

        /* metadata */
        fprintf(stream, "\t< cur. queue size : %zu >\n"\
                        "\t< max. queue size : %zu >\n"\
                        "\t< max. item  size : %zu >\n"\
                        "\t< queue buf. size : %zu >\n",
                queue->cur_q_size, queue->max_q_size,
                queue->max_item_size, queue->buffer_size);

        /* data */
        size_t sz = queue->cur_q_size;
        while (sz != 0) {
                if (q_ptr == (queue->buffer + queue->buffer_size)) {
                        q_ptr = queue->buffer;
                }
                memcpy(buf, q_ptr + sizeof(size_t), (size_t)(*q_ptr));
                buf[(size_t)(*q_ptr)] = '\0';
                fprintf(stream, "\t|--> %zu %s \n", (size_t)(*q_ptr), buf);
                q_ptr += sizeof(size_t) + queue->max_item_size;
                --sz;
        }

        fflush(stream);

        pthread_mutex_unlock(&queue->mutex);

        return 0;
}

/* TODO: add tests where more than 1 thread uses the same queue */

int test_queue(char *err_msg) {
        FILE *stream;
        queue_t *queue = NULL;

        queue = queue_alloc(QUEUE_MAX_SIZE, ITEM_MAX_SIZE);
        if (queue == NULL) {
                strcpy(err_msg, "'queue_alloc' failed");
                goto err;
        }

        if (!queue_empty(queue)) {
                strcpy(err_msg, "'queue_empty' failed; queue should be empty");
                goto err;
        }

        int i = 0;
        for (i = 0; i < QUEUE_MAX_SIZE; i++) {
                if(queue_push(queue, item[i], strlen(item[i]) + 1)) {
                        strcpy(err_msg, "'queue_push' failed; ordinary case");
                        goto err;
                }

                if (queue_empty(queue)) {
                        strcpy(err_msg, "'queue_empty' failed; queue should not be empty");
                        goto err;;
                }
        }
        /* here i equals QUEUE_MAX_SIZE */

        if (!queue_full(queue)) {
                strcpy(err_msg, "'queue_full' failed; queue should be full");
                goto err;
        }

        if (!queue_push(queue, item[i], strlen(item[i]) + 1)) {
                strcpy(err_msg, "'queue_push' failed; queue had to return error");
                goto err;
        }

        for (int j = 0; j < QUEUE_MAX_SIZE - 1; j++) {
                size_t it_sz = 0;
                char *it = queue_front(queue, &it_sz);

                if (strcmp(item[j], it) || it_sz != (strlen(item[j]) + 1)) {
                        strcpy(err_msg, "'queue_front' failed; wrong return result");
                        goto err;
                }

                if (queue_pop(queue)) {
                        strcpy(err_msg, "'queue_pop' failed; ordinary case");
                        goto err;
                }
        }

        size_t j = i;
        for (; j < i + QUEUE_MAX_SIZE - 1; j++) {
                if (queue_push(queue, item[j], strlen(item[j]) + 1)) {
                        strcpy(err_msg, "'queue_push' failed; ordinary case");
                        goto err;
                }
        }
        /* need to swap values of i and j because both will be used later in the code */
        i ^= j;
        j ^= i;
        i ^= j;

        if (!queue_full(queue)) {
                strcpy(err_msg, "'queue_full' failed; queue should be full");
                goto err;
        }

        if (queue_pop(queue)) {
                        strcpy(err_msg, "'queue_pop' failed; ordinary case");
                        goto err;
        }

        if (!queue_push(queue, item[i], strlen(item[i]) + 1)) {
                strcpy(err_msg, "'queue_push' failed; function had to return error");
                goto err;
        }

        for (i = 0; i < QUEUE_MAX_SIZE - 1; i++) {
                size_t it_sz = 0;
                char *it = queue_front(queue, &it_sz);

                if (strcmp(item[j], it) || it_sz != (strlen(item[j]) + 1)) {
                        strcpy(err_msg, "'queue_front' failed; wrong return result");
                        goto err;
                }
                ++j;

                if(queue_pop(queue)) {
                        strcpy(err_msg, "'queue_pop' failed; ordinary case");
                        goto err;
                }

                if (queue_full(queue)) {
                        strcpy(err_msg, "'queue_full' failed; queue should not be full");
                        goto err;
                }

                if ((i != (QUEUE_MAX_SIZE - 2)) && queue_empty(queue)) {
                        strcpy(err_msg, "'queue_empty' failed; queue should not be empty");
                        goto err;
                }
        }

        if (!queue_empty(queue)) {
                strcpy(err_msg, "'queue_empty' failed; queue should be empty");
                goto err;
        }

        if(!queue_pop(queue)) {
                strcpy(err_msg, "'queue_pop' failed; function had to return error");
                goto err;
        }

        queue_free(queue);

        return 0;

err:
        stream = fopen("./test-queue.dump", "w");
        if (!stream) {
                return -1;
        }

        queue_print(stream, queue); /* print out queue on error case */
        queue_free(queue);

        fclose(stream); /* does not really want to know if fclose fail here or not */

        return -1;
}
