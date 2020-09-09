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
static struct SignalSemaphore sem_mask;

// driver config
ULONG midi_drv_sysex_max_size = MIDI_DRV_DEFAULT_SYSEX_SIZE;

// port data
struct port_data {
    midi_drv_tx_func_t tx_func;
    midi_drv_rx_func_t rx_func;
    struct MidiPortData port_data;
    APTR user_data;
    struct midi_parser_handle parser;
    struct SignalSemaphore sem_port;
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
    
    ObtainSemaphore(&pd->sem_port);
    while(1) {
        // port was closed
        if(pd->tx_func == NULL) {
            D(("TX: closed...\n"));
            break;
        }
        ULONG data = pd->tx_func(pd->user_data);
        // no more data
        if(data == 0x100) {
            break;
        }
        D(("TX: %02lx\n", data));
        int res = midi_parser_feed(&pd->parser, data);
        // send regular message
        if(res == MIDI_PARSER_RET_MSG) {
            midi_msg_t msg = pd->parser.msg;
            midi_drv_msg_t dmsg = {
                .port = portnum,
                .midi_msg = msg
            };
            D(("TX: #%ld msg %08lx\n", portnum, msg));
            midi_drv_api_tx_msg(&dmsg);
        }
        // send sysex
        else if(res == MIDI_PARSER_RET_SYSEX_OK) {
            D(("TX: sysex %ld\n", pd->parser.sysex_bytes));
            midi_msg_t msg = pd->parser.msg;
            midi_drv_msg_t dmsg = {
                .port = portnum,
                .midi_msg = msg,
                .sysex_data = pd->parser.sysex_buf,
                .sysex_size = pd->parser.sysex_bytes
            };
            midi_drv_api_tx_msg(&dmsg);
        }
    }
    ReleaseSemaphore(&pd->sem_port);
}

static void port_recv(midi_drv_msg_t *msg)
{
    if(msg->port < MIDI_DRV_NUM_PORTS) {
        struct port_data *pd = &ports[msg->port];
        
        ObtainSemaphore(&pd->sem_port);
        
        // port was closed?
        if(pd->rx_func == NULL) {
            D(("RX: closed...\n"));
            return;
        }

        // is sysex?
        if(msg->midi_msg.b[MIDI_MSG_STATUS] == MS_SysEx) {
            if((msg->sysex_data == NULL) || (msg->sysex_size == 0)) {
                D(("RX: no sysex data??\n"));
                return;
            }
            ULONG num_bytes = msg->sysex_size;
            UBYTE *data = msg->sysex_data;
            for(ULONG i=0;i<num_bytes;i++) {
                D(("RXs: #%ld %02lx\n", msg->port, data[i]));
                pd->rx_func(data[i], pd->user_data);
            }
        }
        // regular message
        else {
            int num_bytes = msg->midi_msg.b[MIDI_MSG_SIZE];
            for(int i=0;i<num_bytes;i++) {
                D(("RX: #%ld %02lx\n", msg->port, msg->midi_msg.b[i]));
                pd->rx_func(msg->midi_msg.b[i], pd->user_data);
            }
        }

        ReleaseSemaphore(&pd->sem_port);

    } else {
        D(("RX: invalid port: %ld\n", msg->port));
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
        midi_drv_msg_t *msg = NULL;
        // block and get next message or return with signal
        int res = midi_drv_api_rx_msg(&msg, &got_sig);
        D(("midi: rx_msg res=%ld, mask=%08lx\n", res, got_sig));

        // own signal was received
        if(res == MIDI_DRV_RET_OK_SIGNAL) {
            // shutdown
            if((got_sig & SIGBREAKF_CTRL_C) == SIGBREAKF_CTRL_C) {
                break;
            }

            // activate port for transmit
            if((got_sig & worker_sigmask) == worker_sigmask) {
                ULONG mask;
                
                // fetch current mask
                ObtainSemaphore(&sem_mask);
                mask = activate_portmask;
                activate_portmask = 0;
                ReleaseSemaphore(&sem_mask);

                D(("midi: activate port mask: %08lx\n", mask));
                for(int i=0;i<MIDI_DRV_NUM_PORTS;i++) {
                    if(mask & (1<<i)) {
                        port_xmit(i);
                    }
                }
            }
        }
        else if(res == MIDI_DRV_RET_OK) {
            if(msg != NULL) { 
                port_recv(msg);
                midi_drv_api_rx_msg_done(msg);
            }
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
    
    STRPTR name = midi_drv_api_config();

    InitSemaphore(&sem_mask);

    // init ports
    for(int i=0;i<MIDI_DRV_NUM_PORTS;i++) {
        ports[i].tx_func = NULL;
        ports[i].rx_func = NULL;
        ports[i].port_data.ActivateXmit = ActivateXmit;
        InitSemaphore(&ports[i].sem_port);
    }

    // main signal for worker sync
    main_sig = AllocSignal(-1);
    if(main_sig == -1) {
        D(("midi: main sig alloc failed!\n"));
        return FALSE;
    }

    main_task = FindTask(NULL);

    // setup worker task
    worker_task = CreateTask(name, 0L, (void *)task_main, 6000L);
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
        ObtainSemaphore(&ports[portnum].sem_port);
        ports[portnum].tx_func = tx_func;
        ports[portnum].rx_func = rx_func;
        ports[portnum].user_data = user_data;
        ReleaseSemaphore(&ports[portnum].sem_port);
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
        ObtainSemaphore(&ports[portnum].sem_port);
        ports[portnum].tx_func = NULL;
        ports[portnum].rx_func = NULL;
        ReleaseSemaphore(&ports[portnum].sem_port);
        midi_parser_exit(&ports[portnum].parser);
    }
}

static SAVEDS ASM void ActivateXmit(REG(a2, APTR userdata),REG(d0, LONG portnum))
{
    D(("midi: ActivateXMit(%ld)\n", portnum));
    ObtainSemaphore(&sem_mask);
    activate_portmask |= 1 << portnum;
    ReleaseSemaphore(&sem_mask);
    Signal(worker_task, 1 << worker_sig); 
}
