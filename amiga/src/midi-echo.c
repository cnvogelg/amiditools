#define USE_INLINE_STDARG

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/camd.h>

#include <midi/camd.h>
#include <utility/tagitem.h>

#include "midi-setup.h"

static const char *TEMPLATE = 
   "IN/A,OUT/A,"
   "SMS/SYSEXMAXSIZE/K/N,"
   "V/VERBOSE/S";
typedef struct {
    char *in_cluster;
    char *out_cluster;
    ULONG *sysex_max_size;
    LONG *verbose;
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

    /* max size for sysex */
    ULONG sysex_max_size = 2048;
    if(params.sysex_max_size != NULL) {
        sysex_max_size = *params.sysex_max_size;
    }

    /* midi open */
    ms.tx_name = params.out_cluster;
    ms.rx_name = params.in_cluster;
    ms.sysex_max_size = sysex_max_size;
    int res = midi_open(&ms);
    if(res != 0) {
        midi_close(&ms);
        return res;
    }

    /* main loop */
    Printf("midi-echo: from '%s' to '%s' running. Press Ctrl+C to abort.\n",
        params.in_cluster, params.out_cluster);
    while(alive) {
        sigmask = Wait(SIGBREAKF_CTRL_C | 1L<<ms.rx_sig);
        if(sigmask & SIGBREAKF_CTRL_C)
            alive=FALSE;
        while(GetMidi(ms.node, &msg)) {
            if(params.verbose) {
                Printf("%08ld: %08lx\n", msg.mm_Time, msg.mm_Msg);
            }
            PutMidi(ms.tx_link, msg.mm_Msg);
        }
    }

    midi_close(&ms);

    FreeArgs(args);

    CloseLibrary((struct Library *)DOSBase);
    return 0;
}
