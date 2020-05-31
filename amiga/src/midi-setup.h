#ifndef MIDI_SETUP_H
#define MIDI_SETUP_H

extern struct Library *CamdBase;

struct MidiSetup {
    /* input */
    char            *rx_name;
    char            *tx_name;
    ULONG            sysex_max_size;
    /* state */
    struct MidiNode *node;
    struct MidiLink *rx_link;
    struct MidiLink *tx_link;
    UBYTE           *sysex_buf;
    BYTE             rx_sig;
};

extern int midi_open(struct MidiSetup *ms);
extern void midi_close(struct MidiSetup *ms);

#endif