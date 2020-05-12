/*
 * a CAMD midi driver for classic Amigas
 */

#include <proto/exec.h>
#include <proto/alib.h>
#include <libraries/bsdsocket.h>
#include <midi/camddevices.h>
#include <midi/camd.h>

#include "debug.h"
#include "compiler.h"
#include "udp.h"
#include "proto.h"
#include "parser.h"

#define NUM_PORTS 4

typedef ULONG (* ASM tx_func_t)(REG(a2, APTR) userdata);
typedef void (* ASM rx_func_t)(REG(d0, UWORD input), REG(a2, APTR userdata));

static SAVEDS ASM void ActivateXmit(REG(a2, APTR userdata),REG(d0, LONG portnum));

static BOOL ASM Init(REG(a6, APTR sysbase));
static void Expunge(void);
static SAVEDS ASM struct MidiPortData *OpenPort(
					 REG(a3, struct MidiDeviceData *data),
					 REG(d0, LONG portnum),
					 REG(a0, tx_func_t tx_func),
					 REG(a1, rx_func_t rx_func),
					 REG(a2, APTR userdata)
					 );
static ASM void ClosePort(
		   REG(a3, struct MidiDeviceData *data),
		   REG(d0, LONG portnum)
		   );

int main()
{
    /* do not run driver */
    return -1;
}

/* midi driver structure */
static struct MidiDeviceData my_dev = {
    .Magic = MDD_Magic,
    .Name = "udp",
    .IDString = "UDP network midi",
    .Version = 0,
    .Revision = 1,
    .Init = (APTR)Init,
    .Expunge = Expunge,
    .OpenPort = OpenPort,
    .ClosePort = ClosePort,
    .NPorts = NUM_PORTS,
    .Flags = 1, // new style driver
};

struct ExecBase *SysBase;
static struct Task *main_task;
static struct Task *worker_task;
static BYTE main_sig;
static BYTE worker_sig;
static BOOL worker_status;
static int activate_portmask;
static struct SignalSemaphore sem;

// UDP config
static struct proto_handle proto = {
    .server_name = "localhost",
    .client_name = "localhost",
    .server_port = 6820,
    .client_port = 6821
};

// port data
struct port_data {
    tx_func_t tx_func;
    rx_func_t rx_func;
    struct MidiPortData port_data;
    APTR user_data;
    struct parser_handle parser;
};
struct port_data ports[NUM_PORTS];

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
        if(parser_feed(&pd->parser, data)) {
            MidiMsg msg;
            msg.l[0] = pd->parser.msg;
            D(("cmd %08lx\n", msg.l[0]));

            int res = proto_send_msg(&proto, &msg);
            if(res != 0) {
                D(("send err: %ld\n", res));
            }
        }
    }
}

static void port_recv(void)
{
    MidiMsg msg;
    int res = proto_recv_msg(&proto, &msg);
    if(res == 0) {
        int port = msg.mm_Port;
        D(("port recv %ld: %08lx\n", port, msg));
        if(port < NUM_PORTS) {
            struct port_data *pd = &ports[port];
            UBYTE status = msg.mm_Status;
            int num_bytes = parser_midi_len(status);
            for(int i=0;i<num_bytes;i++) {
                D(("rx: %02lx\n", msg.b[i]));
                pd->rx_func(msg.b[i], pd->user_data);
            }
        }
    } else {
        D(("recv err: %ld\n", res));
    }
}

static void main_loop(void)
{
    D(("midi: main loop\n"));
    
    // main loop
    ULONG worker_sigmask = 1 << worker_sig;
    D(("midi: worker mask=%08lx\n", worker_sigmask));
    while(1) {
        ULONG got_sig = SIGBREAKF_CTRL_C | worker_sigmask;
        int res = proto_wait_recv(&proto, 0, &got_sig);
        D(("midi: wait res=%ld, mask=%08lx\n", res, got_sig));

        // abort
        if(res == -1) {
            break;
        }
        else if(res == 0) {
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
                for(int i=0;i<NUM_PORTS;i++) {
                    if(mask & (1<<i)) {
                        port_xmit(i);
                    }
                }
            }
        }
        else {
            port_recv();
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
        D(("midi: no worker_sig!\n"));
        goto init_done;
    }

    if(proto_init(&proto, SysBase) != 0) {
        D(("midi: proto_init failed!\n"));
        goto init_done;
    }

    worker_status = TRUE;

init_done:
    // report back that init is done
    D(("midi task: Init done.\n"));
    Signal(main_task, main_sig);

    // enter main loop
    main_loop();

    // shutdown
    D(("midi task: Shutdown.\n"));

    proto_exit(&proto);

    FreeSignal(worker_sig);

    // report back that we are done
    Signal(main_task, main_sig);
}

/* Driver Functions */

static BOOL ASM Init(REG(a6, APTR sysbase))
{
    D(("midi: Init\n"));
    SysBase = sysbase;
    
    InitSemaphore(&sem);

    // init ports
    for(int i=0;i<NUM_PORTS;i++) {
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
    worker_task = CreateTask("midi.udp", 50L, (void *)task_main, 6000L);
    if(worker_task == NULL) {
        D(("midi: create task failed!\n"));
        return FALSE;
    }

    // wait for worker init status
    Wait(main_sig);

    D(("midi: Init done: status=%ld\n", (ULONG)worker_status));
    return worker_status;
}

static void Expunge(void)
{
    D(("midi: Expunge\n"));

    // signal end and wait for shutdown
    Signal(worker_task, SIGBREAKF_CTRL_C);
    Wait(main_sig);

    FreeSignal(main_sig);

    D(("midi: Expunge done\n"));
}

static SAVEDS ASM struct MidiPortData *OpenPort(
					 REG(a3, struct MidiDeviceData *data),
					 REG(d0, LONG portnum),
					 REG(a0, tx_func_t tx_func),
					 REG(a1, rx_func_t rx_func),
					 REG(a2, APTR user_data)
					 )
{
    D(("midi: OpenPort(%ld)\n", portnum));
    if(portnum < NUM_PORTS) {
        if(parser_init(&ports[portnum].parser, SysBase, (UBYTE)portnum)!=0) {
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

static ASM void ClosePort(
		   REG(a3, struct MidiDeviceData *data),
		   REG(d0, LONG portnum)
		   )
{
    D(("midi: ClosePort(%ld)\n", portnum));
    if(portnum < NUM_PORTS) {
        ObtainSemaphore(&sem);
        ports[portnum].tx_func = NULL;
        ports[portnum].rx_func = NULL;
        ReleaseSemaphore(&sem);
        parser_exit(&ports[portnum].parser);
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
