#define USE_INLINE_STDARG

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/camd.h>

#include <midi/camd.h>
#include <midi/mididefs.h>
#include <utility/tagitem.h>

#include "midi-setup.h"

static const char *TEMPLATE = 
   "OUT/A";
typedef struct {
  char *out_cluster;
} params_t;
static params_t params;

struct DosLibrary *DOSBase;

int main(int argc, char **argv)
{
    struct RDArgs *args;
    struct MidiSetup ms;
    MidiMsg msg;
    BOOL alive = TRUE;
    ULONG sigmask;

    DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 0L);

    /* First parse args */
    args = ReadArgs(TEMPLATE, (LONG *)&params, NULL);
    if(args == NULL) {
        PrintFault(IoErr(), "Args Error");
        return RETURN_ERROR;
    }

    /* midi open */
    ms.tx_name = params.out_cluster;
    ms.rx_name = NULL;
    int res = midi_open(&ms);
    if(res != 0) {
        midi_close(&ms);
        return res;
    }

    /* main loop */
    Printf("midi-send: to '%s'\n", params.out_cluster);

    Printf("NoteOn\n");
    msg.mm_Status = MS_NoteOn;
    msg.mm_Data1 = 0x42; // note
    msg.mm_Data2 = 0x11; // velocity
    PutMidi(ms.tx_link, msg.mm_Msg);
 
    Delay(10);

    Printf("NoteOff\n");
    msg.mm_Status = MS_NoteOff;
    msg.mm_Data1 = 0x42; // note
    msg.mm_Data2 = 0x11; // velocity
    PutMidi(ms.tx_link, msg.mm_Msg);

    midi_close(&ms);

    FreeArgs(args);

    CloseLibrary((struct Library *)DOSBase);
    return 0;
}
