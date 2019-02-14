/**
 * Copyright (C) 2016, 2017  Sergey Morozov <sergey@morozov.ch>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>

#include "test.h"

static struct test_case {
        const char *name;
        int (*func)(char *);
} test_suit[] = {
        { "conf",  test_conf },
        { "log",   test_log },
        { "queue", test_queue },
};

int main(int argc, char *argv[]) {
        /* number of test cases */
        int tst_cases = sizeof(test_suit) / sizeof(struct test_case);
        int fail_cnt = 0; /* counter of failed cases */
        char err_msg[BUFSIZ]; /* buffer for error message */

        printf("START TEST SUIT (%d test cases)\n", tst_cases);

        int res;
        for (size_t i = 0; i < tst_cases; i++) {
                printf("\n\t%s: ", test_suit[i].name);
                fflush(stdout);

                res = test_suit[i].func(err_msg);

                printf("%s\n", res ? "failure" : "success");
                if (res) {
                        printf("\t\tREASON: %s\n", err_msg);
                        ++fail_cnt;
                }
                fflush(stdout);
        }

        printf("\n\tRESULT:\n\t\tsuccesses: %d\n\t\tfailures:  %d\n",
               tst_cases - fail_cnt,
               fail_cnt);

        printf("FINISH TEST SUIT (%d test cases)\n", tst_cases);

        if (fail_cnt == 0)  {
                return EXIT_SUCCESS;
        }
        /* else (there were failures) */
        return EXIT_FAILURE;
}
