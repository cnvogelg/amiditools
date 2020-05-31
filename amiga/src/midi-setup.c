#define USE_INLINE_STDARG

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/camd.h>

#include <midi/camd.h>
#include <utility/tagitem.h>

#include "midi-setup.h"

struct Library *CamdBase;

int midi_open(struct MidiSetup *ms)
{
    ms->rx_sig = -1;
    ms->node = NULL;
    ms->rx_link = NULL;
    ms->tx_link = NULL;

    /* sysex buf */
    ms->sysex_buf = AllocVec(ms->sysex_max_size, 0);
    if(ms->sysex_buf == NULL) {
        PutStr("No memory for sysec buffer!\n");
        return 6;
    }

    CamdBase = OpenLibrary((UBYTE *)"camd.library", 0L);
    if(CamdBase == NULL) {
        PutStr("Error opening 'camd.library'!\n");
        return 1;
    }

    /* alloc rx signal */
    ms->rx_sig = AllocSignal(-1);
    if(ms->rx_sig == -1) {
        PutStr("No Signal!\n");
        return 2;
    }

    /* create midi node */
    ms->node = CreateMidi(MIDI_MsgQueue, 2048L,
                MIDI_SysExSize, ms->sysex_max_size,
                MIDI_RecvSignal, ms->rx_sig,
                MIDI_Name, "midi-echo",
                TAG_END);
    if(ms->node == NULL) {
        PutStr("Error creating midi node!\n");
        return 3;
    }

    /* rx_link */
    if(ms->rx_name != NULL) {
        ms->rx_link = AddMidiLink(ms->node, MLTYPE_Receiver, 
            MLINK_Location, ms->rx_name,
            TAG_END);
        if(ms->rx_link == NULL) {
            Printf("Error creating rx link to '%s'\n", ms->rx_name);
            return 4;
        }
    }

    /* tx_link */
    if(ms->tx_name != NULL) {
        ms->tx_link = AddMidiLink(ms->node, MLTYPE_Sender,
            MLINK_Location, ms->tx_name,
            TAG_END);
        if(ms->tx_link == NULL) {
            Printf("Error creating tx link to '%s'\n", ms->tx_name);
            return 5;
        }
    }

    /* all ok */
    return 0;
}

void midi_close(struct MidiSetup *ms)
{
    if(ms->tx_link != NULL) {
        RemoveMidiLink(ms->tx_link);
    }
    if(ms->rx_link != NULL) {
        RemoveMidiLink(ms->rx_link);
    }
    if(ms->node != NULL) {
        DeleteMidi(ms->node);
    }
    if(ms->rx_sig != -1) {
        FreeSignal(ms->rx_sig);
    }
    if(CamdBase != NULL) {
        CloseLibrary(CamdBase);
    }
    if(ms->sysex_buf != NULL) {
        FreeVec(ms->sysex_buf);
    }
}
