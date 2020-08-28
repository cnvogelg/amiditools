/*
 * a CAMD midi driver for classic Amigas
 */

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/alib.h>
#include <midi/camddevices.h>
#include <midi/camd.h>
#include <midi/mididefs.h>

#include "debug.h"
#include "compiler.h"
#include "midi-msg.h"
#include "midi-parser.h"
#include "midi-drv.h"

static SAVEDS ASM void ActivateXmit(REG(a2, APTR userdata),REG(d0, LONG portnum));

int main()
{
    /* do not run driver */
    return -1;
}

struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
static struct Task *main_task;
static struct Task *worker_task;
static BYTE main_sig;
static BYTE worker_sig;
static BOOL worker_status;
static int activate_portmask;
static struct SignalSemaphore sem;

// driver config
ULONG midi_drv_sysex_max_size = MIDI_DRV_DEFAULT_SYSEX_SIZE;

// port data
struct port_data {
    midi_drv_tx_func_t tx_func;
    midi_drv_rx_func_t rx_func;
    struct MidiPortData port_data;
    APTR user_data;
    struct midi_parser_handle parser;
};
struct port_data ports[MIDI_DRV_NUM_PORTS];

/* Config Driver */

int midi_drv_config(char *cfg_file, char *arg_template,
                    struct midi_drv_config_param *param,
                    midi_config_func_t func)
{
    BPTR cfginput, oldinput;
    struct RDArgs *rda;
    int result = MIDI_DRV_RET_OK;

    D(("config init:\n"));

    /* only need DOS for config */
    if(DOSBase = (struct DosLibrary *)OpenLibrary ("dos.library",36)) {
        D(("dosbase %08lx  file %s\n", DOSBase, cfg_file));
        /* try to read config */
        if(cfginput = Open(cfg_file, MODE_OLDFILE)) {
            D(("opened cfg file: '%s'\n", cfg_file));
            oldinput = SelectInput(cfginput);
            rda = ReadArgs(arg_template, (LONG *)param, NULL);
            if(rda != NULL) {
                D(("got args\n"));
                func(param);
                FreeArgs(rda);
            } else {
                D(("failed parsing args!\n"));
                result = MIDI_DRV_RET_PARSE_ERROR;
            }
            D(("close cfg\n"));
            Close(cfginput);
            SelectInput(oldinput);
        } else {
            D(("cfg file not found: '%s'\n", cfg_file));
            result = MIDI_DRV_RET_FILE_NOT_FOUND;
        }
        CloseLibrary((struct Library *)DOSBase);
    } else {
        D(("no DOS!\n"));
        result = MIDI_DRV_RET_FATAL_ERROR;
    }
    return result;
}

/* Worker Task */

static void port_xmit(int portnum)
{
    D(("port xmit %ld\n", portnum));
    struct port_data *pd = &ports[portnum];
    
    while(1) {
        ULONG data = pd->tx_func(pd->user_data);
        if(data == 0x100) {
            break;
        }
        D(("TX: %02lx\n", data));
        int res = midi_parser_feed(&pd->parser, data);
        // send regular message
        if(res == MIDI_PARSER_RET_MSG) {
            midi_msg_t msg = pd->parser.msg;
            D(("TX: msg %08lx\n", msg));
            midi_drv_api_tx_msg(portnum, msg);
        }
        // send sysex
        else if(res == MIDI_PARSER_RET_SYSEX_OK) {
            D(("TX: sysex %ld\n", pd->parser.sysex_bytes));
            midi_msg_t msg = pd->parser.msg;
            midi_drv_api_tx_sysex(portnum, msg, pd->parser.sysex_buf,
                                  pd->parser.sysex_bytes);
        }
    }
}

static void port_recv(void)
{
    midi_msg_t msg;
    int port;
    int res = midi_drv_api_rx_msg(&port, &msg);
    D(("rx: #%ld res=%ld msg=%08lx\n", port, res, msg));
    if(res != MIDI_DRV_RET_OK) {
        return;
    }
    if(port < MIDI_DRV_NUM_PORTS) {
        struct port_data *pd = &ports[port];
        
        // is sysex?
        if(msg.b[MIDI_MSG_STATUS] == MS_SysEx) {
            ULONG num_bytes = 0;
            UBYTE *buf = NULL;
            midi_drv_api_rx_sysex(port, &buf, &num_bytes);
            if(buf != NULL) {
                for(ULONG i=0;i<num_bytes;i++) {
                    D(("RXs: %02lx\n", buf[i]));
                    pd->rx_func(buf[i], pd->user_data);
                }
            }
        }
        // regular message
        else {
            int num_bytes = msg.b[MIDI_MSG_SIZE];
            for(int i=0;i<num_bytes;i++) {
                D(("RX: %02lx\n", msg.b[i]));
                pd->rx_func(msg.b[i], pd->user_data);
            }
        }
    }
}

static void main_loop(void)
{
    D(("midi: main loop\n"));
    
    // main loop
    ULONG worker_sigmask = 1 << worker_sig;
    D(("midi: worker mask=%08lx\n", worker_sigmask));
    while(1) {
        ULONG got_sig = worker_sigmask | SIGBREAKF_CTRL_C;
        int res = midi_drv_api_rx_wait(&got_sig);
        D(("midi: wait res=%ld, mask=%08lx\n", res, got_sig));

        // own signal was received
        if(res == MIDI_DRV_RET_SIGNAL) {
            // shutdown
            if((got_sig & SIGBREAKF_CTRL_C) == SIGBREAKF_CTRL_C) {
                break;
            }

            // activate port for transmit
            if((got_sig & worker_sigmask) == worker_sigmask) {
                ULONG mask;
                
                // fetch current mask
                ObtainSemaphore(&sem);
                mask = activate_portmask;
                activate_portmask = 0;
                ReleaseSemaphore(&sem);

                D(("midi: activate port mask: %08lx\n", mask));
                for(int i=0;i<MIDI_DRV_NUM_PORTS;i++) {
                    if(mask & (1<<i)) {
                        port_xmit(i);
                    }
                }
            }
        }
        else if(res == MIDI_DRV_RET_OK) {
            port_recv();
        }
        // some error
        else {
            D(("midi: abort worker: error=%ld\n", res));
            break;

        }
    }
}

SAVEDS static void task_main(void)
{
    D(("midi task: Begin\n"));
    worker_status = FALSE;

    // alloc worker sig
    worker_sig = AllocSignal(-1);
    if(worker_sig == -1) {
        D(("midi task: no worker_sig!\n"));
        Signal(main_task, main_sig);
        return;
    }

    if(midi_drv_api_init(SysBase) != 0) {
        D(("midi task: proto_init failed!\n"));
        Signal(main_task, main_sig);
        FreeSignal(worker_sig);
        return;
    }

    worker_status = TRUE;

    // report back that init is done
    D(("midi task: Init done.\n"));
    Signal(main_task, main_sig);

    // enter main loop
    main_loop();

    // shutdown
    D(("midi task: Shutdown.\n"));

    midi_drv_api_exit();

    FreeSignal(worker_sig);

    // report back that we are done
    Signal(main_task, main_sig);
}

/* Driver Functions */

SAVEDS BOOL ASM midi_drv_init(REG(a6, APTR sysbase))
{
    D(("midi: Init\n"));
    SysBase = sysbase;
    
    midi_drv_api_config();

    InitSemaphore(&sem);

    // init ports
    for(int i=0;i<MIDI_DRV_NUM_PORTS;i++) {
        ports[i].tx_func = NULL;
        ports[i].rx_func = NULL;
        ports[i].port_data.ActivateXmit = ActivateXmit;
    }

    // main signal for worker sync
    main_sig = AllocSignal(-1);
    if(main_sig == -1) {
        D(("midi: main sig alloc failed!\n"));
        return FALSE;
    }

    main_task = FindTask(NULL);

    // setup worker task
    worker_task = CreateTask("midi.udp", 0L, (void *)task_main, 6000L);
    if(worker_task == NULL) {
        D(("midi: create task failed!\n"));
        return FALSE;
    }

    // wait for worker init status
    Wait(main_sig);

    D(("midi: Init done: status=%ld\n", (ULONG)worker_status));
    return worker_status;
}

SAVEDS ASM void midi_drv_expunge(void)
{
    D(("midi: Expunge\n"));

    // signal end and wait for shutdown
    Signal(worker_task, SIGBREAKF_CTRL_C);
    Wait(main_sig);

    FreeSignal(main_sig);

    D(("midi: Expunge done\n"));
}

SAVEDS ASM struct MidiPortData *midi_drv_open_port(
        REG(a3, struct MidiDeviceData *data),
        REG(d0, LONG portnum),
        REG(a0, midi_drv_tx_func_t tx_func),
        REG(a1, midi_drv_rx_func_t rx_func),
        REG(a2, APTR user_data)
        )
{
    D(("midi: OpenPort(%ld): max_sysex=%ld\n", portnum, midi_drv_sysex_max_size));
    if(portnum < MIDI_DRV_NUM_PORTS) {
        if(midi_parser_init(&ports[portnum].parser, SysBase, (UBYTE)portnum,
                            midi_drv_sysex_max_size)!=0) {
            D(("midi: parser_init failed!\n"));
            return NULL;
        }
        ObtainSemaphore(&sem);
        ports[portnum].tx_func = tx_func;
        ports[portnum].rx_func = rx_func;
        ports[portnum].user_data = user_data;
        ReleaseSemaphore(&sem);
        return &ports[portnum].port_data;
    } else {
        return NULL;
    }
}

SAVEDS ASM void midi_drv_close_port(
        REG(a3, struct MidiDeviceData *data),
        REG(d0, LONG portnum)
        )
{
    D(("midi: ClosePort(%ld)\n", portnum));
    if(portnum < MIDI_DRV_NUM_PORTS) {
        ObtainSemaphore(&sem);
        ports[portnum].tx_func = NULL;
        ports[portnum].rx_func = NULL;
        ReleaseSemaphore(&sem);
        midi_parser_exit(&ports[portnum].parser);
    }
}

static SAVEDS ASM void ActivateXmit(REG(a2, APTR userdata),REG(d0, LONG portnum))
{
    D(("midi: ActivateXMit(%ld)\n", portnum));
    ObtainSemaphore(&sem);
    activate_portmask |= 1 << portnum;
    ReleaseSemaphore(&sem);
    Signal(worker_task, 1 << worker_sig); 
}
