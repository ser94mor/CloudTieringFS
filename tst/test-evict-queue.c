#include <stdio.h>
#include <stdlib.h>

#include "evict-queue.h"
#include "conf.h"

void print_ev_q_info(ev_q *queue) {
    printf("  [queue info]\n");
    if (queue == NULL) {
        printf("    NULL\n");
    } else {
        printf("    { max_size: %zd }\n", queue->max_size);
        printf("    { size: %zd }\n", queue->size);
        printf("    { is head null?: %d }\n", queue->head == NULL);
        printf("    { is tail null?: %d }\n", queue->tail == NULL);
        printf("    { is head equals tail?: %d }\n", queue->tail == NULL);
    }
    printf("  [queue info]\n");
}

void print_ev_n_info(ev_n *node) {
    printf("  [node info]\n");
    if (node == NULL) {
        printf("    NULL\n");
    } else {
        printf("    { file_path: %s }\n", node->file_name);
        printf("    { is next null?: %d }\n", node->next == NULL);
    }
    printf("  [node info]\n");
}

int main(int argc, char *argv[]) {
    ev_n  *node = NULL;
    ev_q *queue = NULL;

    printf("============\n");
    printf("NODE AFTER ALLOC\n");
    node = ev_n_alloc("test_dir", "test_file");

    printf("%s\n", ev_q_str(queue));

    print_ev_n_info(node);
    ev_n_free(node);
    printf("============\n\n");

    printf("============\n");
    printf("QUEUE AFTER ALLOC\n");
    queue = ev_q_alloc(EVICT_QUEUE_MAX_SIZE);
    print_ev_q_info(queue);
    printf("============\n\n");

    printf("============\n");
    printf("QUEUE AFTER PUSH\n");

    printf("%s\n", ev_q_str(queue));

    ev_q_push(queue, ev_n_alloc("/foo", "bar"));
    printf("1st element pushed ('/foo/bar')\n");
    print_ev_q_info(queue);

    printf("%s\n", ev_q_str(queue));

    ev_q_push(queue, ev_n_alloc("/foo/foo", "bar-bar"));
    printf("2nd element pushed ('/foo/foo/bar-bar')\n");
    print_ev_q_info(queue);

    printf("%s\n", ev_q_str(queue));

    printf("looping through all queue elements\n");
    ev_n *cur_node = queue->head;
    while (cur_node != NULL) {
        print_ev_n_info(cur_node);
        cur_node = cur_node->next;
    }
    printf("============\n\n");

    printf("============\n");
    printf("QUEUE AFTER POP\n");
    printf("1st element poped ('/foo')\n");
    print_ev_n_info(ev_q_front(queue));
    ev_q_pop(queue);
    print_ev_q_info(queue);
    printf("2nd element poped ('/foo')\n");
    print_ev_n_info(ev_q_front(queue));
    ev_q_pop(queue);
    print_ev_q_info(queue);
    printf("============\n\n");

    printf("============\n");
    printf("QUEUE FRONT, BACK AND EMPTY\n");
    printf("queue is empty now\n");
    printf("front:\n");
    print_ev_n_info(ev_q_front(queue));
    printf("back:\n");
    print_ev_n_info(ev_q_back(queue));
    printf("empty:\n");
    printf("    is empty?: %d\n", ev_q_empty(queue));
    ev_q_push(queue, ev_n_alloc("/foo", "bar"));
    printf("1st element pushed ('/foo/bar')\n");
    printf("front:\n");
    print_ev_n_info(ev_q_front(queue));
    printf("back:\n");
    print_ev_n_info(ev_q_back(queue));
    printf("empty:\n");
    printf("    is empty?: %d\n", ev_q_empty(queue));
    ev_q_push(queue, ev_n_alloc("/foo/foo", "bar-bar"));
    printf("2nd element pushed ('/foo/foo/bar-bar')\n");
    printf("front:\n");
    print_ev_n_info(ev_q_front(queue));
    printf("back:\n");
    print_ev_n_info(ev_q_back(queue));
    printf("empty:\n");
    printf("    is empty?: %d\n", ev_q_empty(queue));
    printf("============\n\n");

    ev_q_free(queue);

    return EXIT_SUCCESS;
}
