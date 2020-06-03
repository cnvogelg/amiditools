#define USE_INLINE_STDARG

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/camd.h>

#include <midi/camd.h>
#include <midi/mididefs.h>
#include <utility/tagitem.h>

#include "midi-setup.h"

static const char *TEMPLATE = 
   "IN/A,"
   "SMS/SYSEXMAXSIZE/K/N";
typedef struct {
  char *in_cluster;
  ULONG *sysex_max_size;
} params_t;
static params_t params;

struct DosLibrary *DOSBase;

static void handle_sysex(struct MidiNode *node, UBYTE *buf, ULONG buf_size)
{
    if(buf == NULL) {
        PutStr("sysex: SKIP: no buffer!\n");
        SkipSysEx(node);
    } else {
        ULONG size = QuerySysEx(node);
        if(size > buf_size) {
            Printf("sysex: SKIP: too large: %ld!\n", size);
            SkipSysEx(node);
        } else {
            ULONG got_size = GetSysEx(node, buf, buf_size);
            Printf("sysex(%ld): ", got_size);
            for(ULONG i=0;i<got_size;i++) {
                Printf("%02lx ", (ULONG)buf[i]);
            }
            PutStr("\n");
        }
    }
}

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
    ms.sysex_max_size = 2048;
    if(params.sysex_max_size != NULL) {
        ms.sysex_max_size = *params.sysex_max_size;
    }
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
            /* sys ex? */
            if(msg.mm_Status == MS_SysEx) {
                handle_sysex(ms.node, ms.sysex_buf, ms.sysex_max_size);
            }
        }
    }

    midi_close(&ms);

    FreeArgs(args);

    CloseLibrary((struct Library *)DOSBase);
    return 0;
}
