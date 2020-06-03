#include <exec/types.h>
#include <proto/utility.h>
#include <proto/dos.h>
#include <proto/exec.h>

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

char **cmd_exec_cmd_line(char **args, cmd_t *cmd_table, handle_other_func_t func)
{
    if(args == NULL) {
        return NULL;
    }

    while(*args != NULL) {
        char **result = run_cmd(args, cmd_table);
        if(result == NULL) {
            if(func != NULL) {
                result = func(args);
            }
            if(result == NULL) {
                return args;
            }
        }
        args = result;
    }
    return NULL;
}

static char **find_commands(char *buf, int arg_size)
{
    char **args = (char **)AllocVec(arg_size, 0);
    if(args == NULL) {
        PutStr("Out of memory!\n");
        return NULL;
    }
    char **args_ptr = args;
    char *buf_ptr = buf;
    int args_left = arg_size;

    while(1) {
        /* skip white spaces */
        while((*buf_ptr == ' ') || (*buf_ptr == '\t') || (*buf_ptr == '\n'))
            buf_ptr++;
        if(*buf_ptr == '\0') 
            break;
        /* new arg begins */
        if(args_left == 1) {
            // TODO: grow buffer
            PutStr("too many commands\n");
            break;
        }
        /* store arg */
        *args_ptr = buf_ptr;
        args_ptr++;
        args_left--;
        /* skip arg */
        while((*buf_ptr != ' ') && (*buf_ptr != '\t') && (*buf_ptr != '\n')
            && (*buf_ptr != '\x0'))
            buf_ptr++;
        if(*buf_ptr == '\x0')
            break;
        /* terminate arg with zero byte */
        *buf_ptr = '\x0';
        buf_ptr++;
    }
    /* end marker in args */
    *args_ptr = NULL;
    return args;
}

int cmd_exec_file(char *file_name, cmd_t *cmd_table, handle_other_func_t func)
{
    BPTR fh = Open((STRPTR)file_name, MODE_OLDFILE);
    if(fh == NULL) {
        Printf("Error opening command file: %s\n", file_name);
        return 1;
    }

    /* get size */
    Seek(fh, 0, OFFSET_END);
    LONG buf_len = Seek(fh, 0, OFFSET_BEGINNING);

    UBYTE *data = AllocVec(buf_len + 1, 0);
    if(data == NULL) {
        PutStr("Out of memory!\n");
        Close(fh);
        return 2;
    }

    int ret_code = 0;
    LONG got_size = Read(fh, data, buf_len);
    if(got_size == buf_len) {
        data[buf_len] = '\0';

        char **args = find_commands(data, 1024);
        if(args != NULL) {
            char **result = cmd_exec_cmd_line(args, cmd_table, func);
            if(result != NULL) {
                Printf("Error parsing in file '%s': cmd=%s\n", file_name, *result);
                return 3;
            }
            FreeVec(args);
        } else {
            Printf("Error finding commands!\n");
        }

    } else {
        PrintFault(IoErr(), "ReadError");
        ret_code = 3;
    }

    FreeVec(data);

    Close(fh);
    return ret_code;
}
