#ifndef MIDI_SETUP_H
#define MIDI_SETUP_H

extern struct Library *CamdBase;

struct MidiSetup {
    char            *rx_name;
    char            *tx_name;
    struct MidiNode *node;
    struct MidiLink *rx_link;
    struct MidiLink *tx_link;
    BYTE             rx_sig;
};

extern int midi_open(struct MidiSetup *ms);
extern void midi_close(struct MidiSetup *ms);

#endif