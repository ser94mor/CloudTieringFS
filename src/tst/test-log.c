/**
 * Copyright (C) 2016, 2017  Sergey Morozov <sergey94morozov@gmail.com>
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

#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <stdio.h>

#include "log.h"
#include "conf.h"

static jmp_buf restore_point; /* needed to recover from segfault */

static void sigsegv_handler(int signo) {
        if (signo == SIGSEGV) {
                signal(SIGSEGV, SIG_DFL);
                longjmp(restore_point, SIGSEGV);
        }
}

int test_log(char *err_msg) {
        /* this test should be execute after test_conf where
           conf_t and log_t structures will be initialized */
        if (get_conf() == NULL) {
                strcpy(err_msg, "configuration was not initialized (get_conf() returned NULL)");
                return -1;
        }

        /* in our case logger framework should be 'default' which means no actions
           we simply invoke methonds to verify that log_t structure was initialized;
           on error we expect segfauld with high probability */
        if (signal(SIGSEGV, sigsegv_handler) == SIG_ERR) {
                strcpy(err_msg, "failed to register custom signal handler for SIGSEGV");
                return -1;
        }

        int fault_code = setjmp(restore_point);
        if (fault_code != 0) {
                signal(SIGSEGV, SIG_DFL); /* set SEGSEGV handler to default */
                sprintf(err_msg, "SIGSEGV signal was handled; open_log, log or close_log "
                                 "function pointers were not initialized; setjmp() returned: %d", fault_code);
                return -1;
        }


        OPEN_LOG("test");

        LOG(DEBUG, "Hello, %s!", "World");
        LOG(INFO,  "Hello, %s!", "You");
        LOG(ERROR, "Hello, %s!", "Me");

        CLOSE_LOG();

        signal(SIGSEGV, SIG_DFL); /* set SEGSEGV handler to default */

        return 0;
}
