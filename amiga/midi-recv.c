#define USE_INLINE_STDARG

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/camd.h>

#include <midi/camd.h>
#include <utility/tagitem.h>

#include "midi-setup.h"

static const char *TEMPLATE = 
   "IN/A";
typedef struct {
  char *in_cluster;
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
    ms.tx_name = NULL;
    ms.rx_name = params.in_cluster;
    int res = midi_open(&ms);
    if(res != 0) {
        midi_close(&ms);
        return res;
    }

    /* main loop */
    Printf("midi-recv: from '%s'\n", params.in_cluster);
    while(alive) {
        sigmask = Wait(SIGBREAKF_CTRL_C | 1L<<ms.rx_sig);
        if(sigmask & SIGBREAKF_CTRL_C)
            alive=FALSE;
        while(GetMidi(ms.node, &msg)) {
            Printf("%08ld: %08lx\n", msg.mm_Time, msg.mm_Msg);
        }
    }

    midi_close(&ms);

    FreeArgs(args);

    CloseLibrary((struct Library *)DOSBase);
    return 0;
}
