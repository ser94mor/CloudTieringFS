#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <dotconf.h>

/*
 * Variables and functions needed for reading configuration files.
 */
static DOTCONF_CB(cb_list);
static DOTCONF_CB(cb_str);

static const configoption_t options[] = {
        {"ExampleStr", ARG_STR, cb_str, NULL, CTX_ALL},
        {"ExampleList", ARG_LIST, cb_list, NULL, CTX_ALL},
        LAST_OPTION
};

DOTCONF_CB(cb_list)
{
        int i;
        printf("%s:%ld: %s: [  ",
               cmd->configfile->filename, cmd->configfile->line, cmd->name);
        for (i = 0; i < cmd->arg_count; i++)
                printf("(%d) %s  ", i, cmd->data.list[i]);
        printf("]\n");
        return NULL;
}

DOTCONF_CB(cb_str)
{
        printf("%s:%ld: %s: [%s]\n",
               cmd->configfile->filename, cmd->configfile->line,
               cmd->name, cmd->data.str);
        return NULL;
}


int main(int argc, char *argv[]) {
        /* open connection to syslog */
        openlog("tiering:fs-monitor", LOG_PID, LOG_USER);
        syslog(LOG_INFO, "starting file system monitor");

        if (argc != 2) {
                syslog(LOG_ERR, "incorrect number of arguments provided: %d; should be 1", argc);
                exit(EXIT_FAILURE);
        }

        /* reading configuration file provided as an argument */
        configfile_t *configfile = dotconf_create(argv[1], options, NULL, CASE_INSENSITIVE);
        if (!configfile) {
                syslog(LOG_ERR, "unable to proceed further due to configuration read failure");
                return EXIT_FAILURE;
        }

        if (dotconf_command_loop(configfile) == 0)
                fprintf(stderr, "Error reading config file\n");

        dotconf_cleanup(configfile);

        initialize_datastuctures();
        start_filessystem_info_updater_thread();
        start_eviction_queue_updater_thread();
        start_data_evictor_thread();

        LOG(INFO, "file system monitor successfully started");

        return EXIT_SUCCESS;
}
