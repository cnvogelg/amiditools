#include <exec/types.h>
#include <proto/utility.h>
#include <proto/dos.h>
#include <proto/exec.h>

#include "cmd.h"

static cmd_t *find_cmd(char *cmd_name, cmd_t *cmd_table)
{
    cmd_t *cmd = cmd_table;
    while(cmd->name != NULL) {
        int match = Stricmp(cmd->name, cmd_name) == 0;
        if(!match && (cmd->long_name != NULL)) {
            match = Stricmp(cmd->long_name, cmd_name) == 0;
        }
        if(match) {
            return cmd;
        }
        cmd++;
    }
    return NULL;
}

static char **run_cmd(char **cmd_ptr, cmd_t *cmd_table)
{
    /* search command */
    char *cmd_name = cmd_ptr[0];
    cmd_ptr++;
    cmd_t *cmd = find_cmd(cmd_name, cmd_table);

    /* command not found */
    if(cmd == NULL) {
        return NULL;
    }

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
            /* next command start? */
            cmd_t *other_cmd = find_cmd(*cmd_ptr, cmd_table);
            if(other_cmd != NULL) {
                break;
            }
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

static int find_args_in_line(char **buf, char **args_buf, int max_args)
{
    char **args_ptr = args_buf;
    char *buf_ptr = *buf;
    int args_left = max_args;
    int num_args = 0;

    while(1) {
        /* skip white spaces */
        while((*buf_ptr == ' ') || (*buf_ptr == '\t') || (*buf_ptr == '\r'))
            buf_ptr++;
        /* eof */
        if(*buf_ptr == '\x0') {
            *buf = NULL;
            break;
        }
        /* end of line */
        if(*buf_ptr == 0x0a) {
            buf_ptr++;
            *buf = buf_ptr;
            break;
        }
        /* new arg begins */
        if(args_left == 1) {
            PutStr("too many args\n");
            return -1;
        }
        /* store arg */
        *args_ptr = buf_ptr;
        args_ptr++;
        args_left--;
        num_args++;
        /* skip arg */
        while((*buf_ptr != ' ') && (*buf_ptr != '\t') && (*buf_ptr != '\n')
            && (*buf_ptr != '\x0'))
            buf_ptr++;
        /* eof */
        if(*buf_ptr == '\x0') {
            *buf = NULL;
            break;
        }
        /* eol */
        if(*buf_ptr == '\n') {
            *buf_ptr = '\x0';
            buf_ptr++;
            *buf = buf_ptr;
            break;
        }
        /* terminate arg */
        *buf_ptr = '\x0';
        buf_ptr++;
    }
    /* end marker in args */
    *args_ptr = NULL;
    return num_args;
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

    /* alloc file buffer */
    UBYTE *data = AllocVec(buf_len + 1, 0);
    if(data == NULL) {
        PutStr("Out of memory!\n");
        Close(fh);
        return 2;
    }

    /* alloc arg buffer */
    const int max_args = 256;
    char **args_buf = (char **)AllocVec(max_args * sizeof(char *), 0);
    if(args_buf == NULL) {
        PutStr("Out of memory!\n");
        Close(fh);
        return 3;
    }

    /* read file */
    int ret_code = 0;
    LONG got_size = Read(fh, data, buf_len);
    
    /* parse loop */
    if(got_size == buf_len) {
        data[buf_len] = '\0';

        char *ptr = data;
        while(ptr != NULL) {
            int num_args = find_args_in_line(&ptr, args_buf, max_args);
            if(num_args > 0) {
                char **result = cmd_exec_cmd_line(args_buf, cmd_table, func);
                if(result != NULL) {
                    Printf("Error parsing in file '%s': cmd=%s\n", file_name, *result);
                    return 3;
                }
            } else if(num_args < 0) {
                Printf("Error finding commands!\n");
                ret_code = 5;
                break;
            }
        }

    } else {
        PrintFault(IoErr(), "ReadError");
        ret_code = 4;
    }

    FreeVec(data);
    FreeVec(args_buf);

    Close(fh);
    return ret_code;
}
