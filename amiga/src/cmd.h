#ifndef CMD_H
#define CMD_H

typedef struct {
    char *name;
    char *long_name;
    int   num_args;
    int   (*run_cmd)(int num_args, char **args);
} cmd_t;

typedef char ** (*handle_other_func_t)(char **args);

extern char **cmd_exec_cmd_line(char **args, cmd_t *cmd_table, handle_other_func_t f);
extern int cmd_exec_file(char *file_name, cmd_t *cmd_table, handle_other_func_t f);

#endif