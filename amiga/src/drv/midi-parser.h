#ifndef PARSER_H
#define PARSER_H

struct midi_parser_handle {
    struct ExecBase *sysBase;
    int bytes_left;
    int bytes_pos;
    UBYTE port_num;

    ULONG sysex_bytes;
    ULONG sysex_left;
    ULONG sysex_max;
    UBYTE *sysex_buf;

    midi_msg_t msg;
};

#define MIDI_PARSER_RET_NONE                 0
#define MIDI_PARSER_RET_MSG                  1
#define MIDI_PARSER_RET_SYSEX_OK             2
#define MIDI_PARSER_RET_SYSEX_TOO_LARGE      3
#define MIDI_PARSER_RET_ERROR                4
#define MIDI_PARSER_RET_INTERNAL             5

extern int midi_parser_init(struct midi_parser_handle *ph, struct ExecBase *sysBase, UBYTE port_num,
                       ULONG max_sysex_size);
extern void midi_parser_exit(struct midi_parser_handle *ph);

extern int midi_parser_feed(struct midi_parser_handle *ph, UBYTE data);

#endif
