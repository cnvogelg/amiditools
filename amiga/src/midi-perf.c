#define USE_INLINE_STDARG

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/camd.h>
#include <proto/alib.h>

#include <midi/camd.h>
#include <midi/mididefs.h>

#include "drv/compiler.h"
#include "drv/debug.h"
#include "midi-setup.h"
#include "midi-tools.h"

static const char *TEMPLATE = 
    "V/VERBOSE/S,"
    "SMS/SYSEXMAXSIZE/K/N,"
    "OUTDEV/A,"
    "INDEV/A,"
    "NUM/K/N";
typedef struct {
    LONG *verbose;
    ULONG *sysex_max_size;
    char *out_dev;
    char *in_dev;
    ULONG *num_msgs;
} params_t;

extern struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct UtilityBase *UtilityBase;
static params_t params;
static BOOL verbose = FALSE;
static struct MidiSetup midi_setup_tx;
static struct MidiSetup midi_setup_rx;
static ULONG sysex_max_size = 2048;

// task
static BYTE main_sig;
static int worker_status;
static struct Task *main_task;
static struct Task *worker_task;

// perf
static ULONG num_msgs = 32;
static ULONG got_msgs;

typedef union {
    LONG  cmd;
    UBYTE b[4];
} MidiCmd;

static void main_loop(void)
{
    PutStr("sending samples...\n");
    for(int i=0;i<num_msgs;i++) {
        MidiCmd msg;
        msg.mm_Status = 0x90;
        msg.mm_Data1 = i;
        msg.mm_Data2 = 10;
        PutMidi(midi_setup_tx.tx_link, msg.cmd);
    }
    PutStr("waiting for rx\n");
    Wait(1 << main_sig);
    Printf("got msgs: %ld\n", got_msgs);
} 

static void worker_loop(void)
{
    D(("worker: loop\n"));
    ULONG midi_mask = 1 << midi_setup_rx.rx_sig;
    ULONG sig_mask = SIGBREAKF_CTRL_C | midi_mask;
    BOOL stay = TRUE;
    while(stay) {
        ULONG got_sig = Wait(sig_mask);
        D(("got sig: %lx\n", got_sig));
        if((got_sig & SIGBREAKF_CTRL_C) == SIGBREAKF_CTRL_C) {
            break;
        }
  
        if((got_sig & midi_mask) == midi_mask) {
            // get midi messages
            MidiMsg msg;
            while(GetMidi(midi_setup_rx.node, &msg)) {
                D(("#%ld ----- RX: %08ld: %08lx\n", got_msgs, msg.mm_Time, msg.mm_Msg));
                got_msgs ++;
                if(got_msgs == num_msgs) {
                    stay = FALSE;
                    break;
                }
            }
        }
    }
    D(("worker: done\n"));
}

SAVEDS static void task_main(void)
{
    D(("task: init begin: exec=%lx\n", SysBase));

    // midi rx setup
    D(("midi rx setup: %s\n", params.in_dev));
    midi_setup_rx.rx_name = params.in_dev;
    midi_setup_rx.midi_name = "midi-perf-rx";
    midi_setup_rx.sysex_max_size = sysex_max_size;
    if(midi_open(&midi_setup_rx) != 0) {
        midi_close(&midi_setup_rx);
        D(("midi_rx failed!\n"));
        worker_status = 11;
        Signal(main_task, 1 << main_sig);
        return;
    }    

    // report back that init is done
    D(("task: init done\n"));
    Signal(main_task, 1 << main_sig);

    // enter main loop
    worker_loop();

    // shutdown
    D(("task: shutdown\n"));

    midi_close(&midi_setup_rx);

    // report back that we are done
    Signal(main_task, 1 << main_sig);

    D(("task: done\n"));
}

static int task_setup(void)
{
    // main signal for worker sync
    main_sig = AllocSignal(-1);
    if(main_sig == -1) {
        Printf("no main signal!\n");
        return 1;
    }

    main_task = FindTask(NULL);

    // setup worker task
    worker_task = CreateTask("midi-perf-worker", 0L, (void *)task_main, 6000L);
    if(worker_task == NULL) {
        Printf("no worker task!\n");
        FreeSignal(main_sig);
        return 2;
    }

    // wait for worker init status and return it
    Wait(1 << main_sig);
    if(worker_status != 0) {
        FreeSignal(main_sig);
    }
    D(("init: task status=%ld\n", worker_status));
    return worker_status;
}

static void task_shutdown(void)
{
    D(("exit: task shutdown...\n"));
    FreeSignal(main_sig);
}

int main(int argc, char **argv)
{
    struct RDArgs *args;
    int result = RETURN_ERROR;

    /* Open Libs */
    DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 0L);
    if(DOSBase != NULL) {
        UtilityBase = (struct UtilityBase *)OpenLibrary("utility.library", 0);
        if(UtilityBase != NULL) {
            /* First parse args */
            args = ReadArgs(TEMPLATE, (LONG *)&params, NULL);
            if(args == NULL) {
                PrintFault(IoErr(), "Args Error");
            } else {

                if(params.verbose != NULL) {
                    verbose = TRUE;
                }

                if(task_setup() == 0) {
                    /* max size for sysex */
                    if(params.sysex_max_size != NULL) {
                        sysex_max_size = *params.sysex_max_size;
                    }
                    if(params.num_msgs != NULL) {
                        num_msgs = *params.num_msgs;
                    }

                    /* setup midi */
                    Printf("midi-perf: out_dev='%s' in_dev='%s'\n",
                        params.out_dev, params.in_dev);
                    midi_setup_tx.tx_name = params.out_dev;
                    midi_setup_tx.midi_name = "midi-perf-tx";
                    midi_setup_tx.sysex_max_size = sysex_max_size;
                    if(midi_open(&midi_setup_tx) == 0) {

                        main_loop();

                    }
                    midi_close(&midi_setup_tx);

                    task_shutdown();
                    result = RETURN_OK;
                }
            }
            CloseLibrary((struct Library *)UtilityBase);
        }
        CloseLibrary((struct Library *)DOSBase);
    }
    return result;
}
