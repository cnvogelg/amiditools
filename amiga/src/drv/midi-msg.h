#ifndef MIDI_MSG_H
#define MIDI_MSG_H

typedef union  {
    UBYTE  b[4];
    ULONG  l;
} midi_msg_t;

#define MIDI_MSG_STATUS 0
#define MIDI_MSG_DATA1  1
#define MIDI_MSG_DATA2  2
#define MIDI_MSG_SIZE   3

#endif
