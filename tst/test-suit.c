#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "test-suit.h"

static struct test_case {
        const char *name;
        int (*func)(char *);
};

struct test_case test_suit[] = {
        { "readconf", test_readconf }
};

int main(int argc, char *argv[]) {
        int tst_cases = (int)sizeof(test_suit); /* number of test cases */
        int fail_cnt = 0; /* counter of failed cases */
        char err_msg[LINE_MAX]; /* buffer for error message */

        printf("START TEST SUIT (%d test cases)\n", tst_cases);

        int res;
        for (size_t i = 0; i < tst_cases; i++) {
                res = test_suit[i].func(&err_msg);
                printf("\t%s: %s\n", test_suit[i].name(), res ? "failure" : "success");
                if (res) {
                        printf("\t\tREASON: %s\n", err_msg);
                }
                fflush(stdout);
        }

        printf("\tRESULT:\n\t\tsuccesses: %d\n\t\tfailures: %d\n", tst_cases - fail_cnt, fail_cnt);

        printf("FINISH TEST SUIT (%d test cases)\n", tst_cases);

        exit(EXIT_SUCCESS);
}
