#ifndef CMD_H
#define CMD_H

typedef struct {
    char *name;
    char *long_name;
    int   num_args;
    int   (*run_cmd)(int num_args, char **args);
} cmd_t;

extern char *cmd_exec_cmd_line(char **args, cmd_t *cmd_table);

#endif