#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"


int test_queue(char *err_msg) {
        queue_t *queue = queue_alloc(10, 15);
        
        char *str = "Hello, World!";
        size_t size = strlen(str) + 1;
        char * str1 = (char *)malloc(size);
        strcpy(str1, str);
        
        if(queue_push(queue, str1, size)) {
                printf("error o push");
        }
        
        queue_print(stdout, queue);
        
        return 0;
}