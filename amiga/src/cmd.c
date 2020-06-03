#include <exec/types.h>
#include <proto/utility.h>

#include "cmd.h"

static char **run_cmd(char **cmd_ptr, cmd_t *cmd_table)
{
    /* search command */
    char *cmd_name = cmd_ptr[0];
    cmd_ptr++;
    cmd_t *cmd = cmd_table;
    while(cmd->name != NULL) {
        int match = Stricmp(cmd->name, cmd_name) == 0;
        if(!match && (cmd->long_name != NULL)) {
            match = Stricmp(cmd->long_name, cmd_name) == 0;
        }
        if(match) { 
            int num_args = cmd->num_args;
            char **args = cmd_ptr;
            /* skip args */
            if(num_args > 0) {
                for(int i=0;i<num_args;i++) {
                    /* command too short? */
                    if(*cmd_ptr == NULL) {
                        return NULL;
                    }
                    cmd_ptr++;
                }
            } 
            /* munge all args */
            else if(num_args < 0) {
                num_args = 0;
                while(*cmd_ptr != NULL) {
                    cmd_ptr++;
                    num_args++;
                }
            }
            /* run command */
            int result = cmd->run_cmd(num_args, args);
            if(result != 0) {
                return NULL;
            }
            return cmd_ptr;
        }
        cmd++;
    }
    /* command not found! */
    return NULL;
}

char **cmd_exec_cmd_line(char **args, cmd_t *cmd_table)
{
    if(args == NULL) {
        return NULL;
    }

    while(*args != NULL) {
        char **result = run_cmd(args, cmd_table);
        if(result == NULL) {
            return args;
        }
        args = result;
    }
    return NULL;
}
