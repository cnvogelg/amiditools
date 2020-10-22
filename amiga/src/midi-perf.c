#define USE_INLINE_STDARG

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/camd.h>
#include <proto/alib.h>
#include <proto/timer.h>

#include <midi/camd.h>
#include <midi/mididefs.h>

#include "drv/compiler.h"
#include "drv/debug.h"
#include "midi-setup.h"
#include "midi-tools.h"

static int task_setup(void);
static void task_shutdown(void);

static const char *TEMPLATE = 
    "V/VERBOSE/S,"
    "SMS/SYSEXMAXSIZE/K/N,"
    "OUTDEV/A,"
    "INDEV/A,"
    "DELAY/K/N,"
    "NUM/K/N";
typedef struct {
    LONG *verbose;
    ULONG *sysex_max_size;
    char *out_dev;
    char *in_dev;
    ULONG *delay;
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
typedef union {
    LONG  l;
    UBYTE b[4];
} MidiCmd;

typedef struct {
    MidiCmd    cmd;
    struct timeval  ts_send;
    struct timeval  ts_recv;
    ULONG           delta_us;
} Sample;

typedef struct {
    ULONG       min;
    ULONG       max;
    ULONG       avg;
    ULONG       num;
} Statistics;

static ULONG loop_delay = 50;
static ULONG num_msgs = 256;
static ULONG got_msgs;
static ULONG num_errors;
static Sample *samples;


static Sample *create_samples_note_sweep(void)
{
    // sanity check
    if(num_msgs > 256) {
        num_msgs = 256;
    }
    if((num_msgs % 2)==1) {
        num_msgs ++;
    }

    ULONG num_notes = num_msgs >> 1;
    Sample *samples = (Sample *)AllocVec(num_msgs * sizeof(Sample), MEMF_ANY | MEMF_CLEAR);
    if(samples == NULL) {
        return NULL;
    }

    Sample *smp= samples;
    for(ULONG i=0;i<num_notes;i++) {
        smp->cmd.mm_Status = 0x90;
        smp->cmd.mm_Data1 = i;
        smp->cmd.mm_Data2 = 10;
        smp++;
        smp->cmd.mm_Status = 0x80;
        smp->cmd.mm_Data1 = i;
        smp->cmd.mm_Data2 = 10;
        smp++;
    }

    return samples;
}

static void free_samples(Sample *samples)
{
    FreeVec(samples);
}

static void calc_sample_delta(Sample *samples, ULONG num)
{
    Sample *smp = samples;
    for(ULONG i=0;i<num;i++) {
        struct timeval delta = smp->ts_recv;
        SubTime(&delta, &smp->ts_send);

        // limit to micros
        smp->delta_us = delta.tv_micro;
        if(delta.tv_secs > 0) {
            smp->delta_us += delta.tv_secs * 1000000UL;
        }
        smp++;
    }
}

static void calc_sample_stats(Sample *samples, ULONG num_msgs, Statistics *stats)
{
    ULONG min = 0xffffffff;
    ULONG max = 0;
    ULONG sum = 0;
    ULONG num = 0;

    Sample *smp = samples;
    for(ULONG i=0;i<num_msgs;i++) {
        ULONG delta = smp->delta_us;
        if(delta < min) {
            min = delta;
        }
        if(delta > max) {
            max = delta;
        }
        sum += delta;
        num++;
        smp++;
    }

    stats->min = min;
    stats->max = max;
    stats->num = num;
    if(num > 0) {
        stats->avg = sum / num;
    } else {
        stats->avg = 0;
    }
}

static int benchmark_samples(Sample *samples, ULONG num_samples)
{
    if(verbose)
        Printf("sending %ld samples...\n", num_samples);
    Sample *smp = samples;
    for(int i=0;i<num_samples;i++) {
        GetSysTime(&smp->ts_send);
        PutMidi(midi_setup_tx.tx_link, smp->cmd.l);
        smp++;
    }
    if(verbose)
        PutStr("waiting for incoming samples...\n");
    ULONG mask = Wait((1 << main_sig) | SIGBREAKF_CTRL_C);
    if((mask & SIGBREAKF_CTRL_C) == SIGBREAKF_CTRL_C) {
        return 2;
    }
    if(verbose)
        Printf("got samples: %ld, errors: %ld\n", num_samples, num_errors);
    if(num_errors > 0) {
        PutStr("Failed with errors...\n");
        return 1;
    }

    calc_sample_delta(samples, num_msgs);
    Statistics stats;
    calc_sample_stats(samples, num_msgs, &stats);
    Printf("min=%6ld, max=%6ld, avg=%6ld  (#%ld)\n",
        stats.min, stats.max, stats.avg, stats.num);

    return 0;
}

static void main_loop(void)
{
    // create samples
    samples = create_samples_note_sweep();
    if(samples == NULL) {
        PutStr("Error creating samples!\n");
        return;
    }

    // setup worker
    if(task_setup()!=0) {
        PutStr("Error setting up worker!\n");
        free_samples(samples);
        return;
    }

    while(1) {
        int result = benchmark_samples(samples, num_msgs);
        if(result != 0) {
            PutStr("stopping...\n");
            break;
        }

        Delay(loop_delay);
    }

    task_shutdown();
    free_samples(samples);
} 

static void worker_loop(void)
{
    D(("worker: loop\n"));
    ULONG midi_mask = 1 << midi_setup_rx.rx_sig;
    ULONG sig_mask = SIGBREAKF_CTRL_C | midi_mask;
    BOOL stay = TRUE;
    Sample *smp = samples;
    while(stay) {
        ULONG got_sig = Wait(sig_mask);
        //D(("got sig: %lx\n", got_sig));
        if((got_sig & SIGBREAKF_CTRL_C) == SIGBREAKF_CTRL_C) {
            break;
        }
  
        if((got_sig & midi_mask) == midi_mask) {
            // get midi messages
            MidiMsg msg;
            while(GetMidi(midi_setup_rx.node, &msg)) {
                GetSysTime(&smp->ts_recv);
                //D(("#%ld RX: %08lx: %08lx\n", got_msgs, msg.mm_Time, msg.mm_Msg));
                // check message
                if(msg.l[0] != smp->cmd.l) {
                    D(("wanted: %08lx\n", smp->cmd.l));
                    num_errors++;
                    Signal(main_task, 1 << main_sig);
                    break;
                }
                got_msgs ++;
                smp++;
                // all done report back
                if(got_msgs == num_msgs) {
                    Signal(main_task, 1 << main_sig);
                    // reset
                    D(("worker: reset\n"));
                    smp = samples;
                    got_msgs = 0;
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

    D(("task: done\n"));

    Signal(main_task, 1 << main_sig);
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
    Signal(worker_task, SIGBREAKF_CTRL_C);
    Wait(1 << main_sig);
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
            if(midi_tools_init_time()==0) {
                /* First parse args */
                args = ReadArgs(TEMPLATE, (LONG *)&params, NULL);
                if(args == NULL) {
                    PrintFault(IoErr(), "Args Error");
                } else {

                    if(params.verbose != NULL) {
                        verbose = TRUE;
                    }

                    /* max size for sysex */
                    if(params.sysex_max_size != NULL) {
                        sysex_max_size = *params.sysex_max_size;
                    }
                    if(params.num_msgs != NULL) {
                        num_msgs = *params.num_msgs;
                    }
                    if(params.verbose != NULL) {
                        verbose = TRUE;
                    }
                    if(params.delay != NULL) {
                        loop_delay = *params.delay;
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

                    result = RETURN_OK;
                }
                midi_tools_exit_time();
            }
            CloseLibrary((struct Library *)UtilityBase);
        }
        CloseLibrary((struct Library *)DOSBase);
    }
    return result;
}
