#ifndef PARSER_H
#define PARSER_H

struct parser_handle {
    struct ExecBase *sysBase;
    int bytes_left;
    int bytes_pos;
    UBYTE port_num;

    ULONG msg; // msg encodes: len | status | data1 | data2
};

extern int parser_init(struct parser_handle *ph, struct ExecBase *sysBase, UBYTE port_num);
extern void parser_exit(struct parser_handle *ph);

extern int parser_feed(struct parser_handle *ph, UBYTE data);
extern int parser_midi_len(UBYTE status);

#endif
