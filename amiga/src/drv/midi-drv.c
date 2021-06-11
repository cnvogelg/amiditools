/*
 * a CAMD midi driver for classic Amigas
 */

#include <proto/exec.h>
#include <proto/dos.h>
#include <clib/alib_protos.h>
#include <midi/camddevices.h>
#include <midi/camd.h>
#include <midi/mididefs.h>

#include "debug.h"
#include "compiler.h"
#include "midi-msg.h"
#include "midi-parser.h"
#include "midi-drv.h"

SAVEDS ASM void midi_drv_xmit(REG(d0, LONG portnum));

typedef void (*xmit_func_t)(void);

#define XMIT_FUNC_DEF(portnum) \
    static void midi_drv_activate ## portnum (void) { midi_drv_xmit(portnum); }
#define XMIT_FUNC(portnum) \
    midi_drv_activate ## portnum

XMIT_FUNC_DEF(0)
XMIT_FUNC_DEF(1)
XMIT_FUNC_DEF(2)
XMIT_FUNC_DEF(3)
XMIT_FUNC_DEF(4)
XMIT_FUNC_DEF(5)
XMIT_FUNC_DEF(6)
XMIT_FUNC_DEF(7)

static xmit_func_t xmit_funcs[] = {
    XMIT_FUNC(0),
    XMIT_FUNC(1),
    XMIT_FUNC(2),
    XMIT_FUNC(3),
    XMIT_FUNC(4),
    XMIT_FUNC(5),
    XMIT_FUNC(6),
    XMIT_FUNC(7)
};

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
static volatile ULONG activate_portmask;
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

    struct Task *end_task;
    BYTE end_sig;
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
        } else {
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
        }

        ReleaseSemaphore(&pd->sem_port);

    } else {
        D(("RX: invalid port: %ld\n", msg->port));
    }
}

static void notify_end_task(int portnum, struct port_data *port)
{
    struct Task *end_task = port->end_task;
    if(end_task != NULL) {
        BYTE end_sig = port->end_sig;
        ULONG end_mask = 1 << end_sig;
        D(("midi: notify end port=%ld task=%lx mask=%lx\n", portnum, end_task, end_mask));
        Signal(end_task, end_mask);
        port->end_task = NULL;
    }
}

static void do_transmit(void)
{
    ULONG mask;

    // fetch current mask
    ObtainSemaphore(&sem_mask);
    mask = activate_portmask;

    D(("midi: activate port mask: %08lx\n", mask));
    for(int i=0;i<MIDI_DRV_NUM_PORTS;i++) {
        if(mask & (1<<i)) {
            port_xmit(i);
            notify_end_task(i, &ports[i]);
        }
    }

    activate_portmask = 0;
    ReleaseSemaphore(&sem_mask);
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
        if(res != MIDI_DRV_RET_OK) {
            break;
        }

        // always handle signals first
        // shutdown
        if((got_sig & SIGBREAKF_CTRL_C) == SIGBREAKF_CTRL_C) {
            break;
        }
        // tx transmit
        if((got_sig & worker_sigmask) == worker_sigmask) {
            do_transmit();
        }

        // a message was received (rx)
        if(msg != NULL) {
            port_recv(msg);
            midi_drv_api_rx_msg_done(msg);
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
        Signal(main_task, 1 << main_sig);
        return;
    }

    if(midi_drv_api_init(SysBase) != 0) {
        D(("midi task: proto_init failed!\n"));
        Signal(main_task, 1 << main_sig);
        FreeSignal(worker_sig);
        return;
    }

    worker_status = TRUE;

    // report back that init is done
    D(("midi task: Init done.\n"));
    Signal(main_task, 1 << main_sig);

    // enter main loop
    main_loop();

    // shutdown
    D(("midi task: Shutdown.\n"));

    midi_drv_api_exit();

    FreeSignal(worker_sig);

    // report back that we are done
    Signal(main_task, 1 << main_sig);
}

/* Driver Functions */

SAVEDS BOOL ASM midi_drv_init(void)
{
    SysBase = *(struct ExecBase **)4;
    D(("midi: Init: Sysbase=%lx\n", SysBase));

    // open dos
    DOSBase = (struct DosLibrary *)OpenLibrary ("dos.library",36);
    if(DOSBase == NULL) {
        D(("midi: no dos??\n"));
        return FALSE;
    }

    STRPTR name = midi_drv_api_config();

    InitSemaphore(&sem_mask);
    D(("acti: %lx\n", &activate_portmask));

    // init ports
    for(int i=0;i<MIDI_DRV_NUM_PORTS;i++) {
        ports[i].tx_func = NULL;
        ports[i].rx_func = NULL;
        ports[i].port_data.ActivateXmit = xmit_funcs[i];
        ports[i].end_task = NULL;
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
    D(("worker task: %lx\n", worker_task));

    // wait for worker init status
    Wait(1 << main_sig);

    D(("midi: Init done: status=%ld\n", (ULONG)worker_status));
    return worker_status;
}

SAVEDS ASM void midi_drv_expunge(void)
{
    D(("midi: Expunge\n"));

    // signal end and wait for shutdown
    Signal(worker_task, SIGBREAKF_CTRL_C);
    Wait(1 << main_sig);

    FreeSignal(main_sig);

    CloseLibrary((struct Library *)DOSBase);

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
    D(("midi: OpenPort(%ld): max_sysex=%ld task=%lx\n", portnum, midi_drv_sysex_max_size, FindTask(NULL)));
    if(portnum < MIDI_DRV_NUM_PORTS) {
        if(midi_parser_init(&ports[portnum].parser, SysBase, (UBYTE)portnum,
                            midi_drv_sysex_max_size)!=0) {
            D(("midi: parser_init failed!\n"));
            return NULL;
        }
        struct port_data *port = &ports[portnum];
        ObtainSemaphore(&port->sem_port);
        port->tx_func = tx_func;
        port->rx_func = rx_func;
        port->user_data = user_data;
        ReleaseSemaphore(&port->sem_port);
        D(("midi: OpenPort(%ld): done user data=%lx\n", portnum, user_data));
        return &port->port_data;
    } else {
        return NULL;
    }
}

static void wait_for_idle(LONG portnum)
{
    ObtainSemaphore(&sem_mask);
    // quick check if its already idle
    ULONG mask = activate_portmask;
    if((mask & (1 << portnum)) == 0) {
        ReleaseSemaphore(&sem_mask);
        D(("midi: is idle\n"));
    } else {
        // we have to wait
        struct port_data *port = &ports[portnum];
        port->end_task = FindTask(NULL);
        port->end_sig = AllocSignal(-1);
        if(port->end_sig == -1) {
            D(("midi: FATAL no sig for idle wait!\n"));
            return;
        }
        ReleaseSemaphore(&sem_mask);

        ULONG wait_mask = 1 << port->end_sig;
        D(("midi: wait for idle: task=%lx mask=%lx\n", port->end_task, wait_mask));
        Wait(wait_mask);
        D(("midi: now is idle\n"));
        FreeSignal(port->end_sig);
    }
}

SAVEDS ASM void midi_drv_close_port(
        REG(a3, struct MidiDeviceData *data),
        REG(d0, LONG portnum)
        )
{
    D(("midi: ClosePort(%ld): task=%lx\n", portnum, FindTask(NULL)));
    if(portnum < MIDI_DRV_NUM_PORTS) {
        struct port_data *port = &ports[portnum];
        wait_for_idle(portnum);
        ObtainSemaphore(&port->sem_port);
        port->tx_func = NULL;
        port->rx_func = NULL;
        ReleaseSemaphore(&port->sem_port);
        midi_parser_exit(&port->parser);
    }
    D(("midi: ClosePort(%ld): done\n", portnum));
}

SAVEDS ASM void midi_drv_xmit(REG(d0, LONG portnum))
{
    D(("midi: ActivateXMit(%ld): task=%lx mask=%lx\n", portnum, FindTask(NULL), &activate_portmask));
    if(portnum < MIDI_DRV_NUM_PORTS) {
        ObtainSemaphore(&sem_mask);
        activate_portmask |= 1 << portnum;
        ReleaseSemaphore(&sem_mask);
        Signal(worker_task, 1 << worker_sig);
    }
    D(("midi: ActivateXMit(%ld): done\n", portnum));
}