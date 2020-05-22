/*
 * a CAMD midi driver for classic Amigas
 */

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/alib.h>
#include <libraries/bsdsocket.h>
#include <midi/camddevices.h>
#include <midi/camd.h>
#include <midi/mididefs.h>

#include "debug.h"
#include "compiler.h"
#include "udp.h"
#include "proto.h"
#include "midi-parser.h"

#define NUM_PORTS 8
#define DEFAULT_SYSEX_SIZE 2048

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
struct DosLibrary *DOSBase;
static struct Task *main_task;
static struct Task *worker_task;
static BYTE main_sig;
static BYTE worker_sig;
static BOOL worker_status;
static int activate_portmask;
static struct SignalSemaphore sem;

// UDP config
#define NAME_LEN 80
static char server_name[NAME_LEN] = "localhost";
static char client_name[NAME_LEN] = "localhost";
static struct proto_handle proto = {
    .server_name = server_name,
    .client_name = client_name,
    .server_port = 6820,
    .client_port = 6821
};

// more config
static ULONG sysex_max_size = DEFAULT_SYSEX_SIZE;

// port data
struct port_data {
    tx_func_t tx_func;
    rx_func_t rx_func;
    struct MidiPortData port_data;
    APTR user_data;
    struct midi_parser_handle parser;
};
struct port_data ports[NUM_PORTS];

/* Config Driver */

#define CONFIG_FILE "ENV:midi/udp.config"
#define ARG_TEMPLATE \
    "CLIENT_HOST/K,CLIENT_PORT/K/N," \
    "SERVER_HOST/K,SERVER_PORT/K/N," \
    "SYSEX_SIZE/K/N"
struct ConfigParam {
    STRPTR client_host;
    ULONG *client_port;
    STRPTR server_host;
    ULONG *server_port;
    ULONG *sysex_size;
};

static void parse_args(struct ConfigParam *param)
{
    if(param->client_host != NULL) {
        D(("set client host: %s\n", param->client_host));
        strncpy(client_name, param->client_host, NAME_LEN);
    }
    if(param->client_port != NULL) {
        D(("set client port: %ld\n", *param->client_port));
        proto.client_port = *param->client_port;
    }
    if(param->server_host != NULL) {
        D(("set server host: %s\n", param->server_host));
        strncpy(server_name, param->server_host, NAME_LEN);
    }
    if(param->server_port != NULL) {
        D(("set server port: %ld\n", *param->server_port));
        proto.server_port = *param->server_port;
    }
    if(param->sysex_size != NULL) {
        D(("set sysex size: %ld\n", *param->sysex_size));
        sysex_max_size = *param->sysex_size;
    }
}

static void config_driver(void)
{
    BPTR cfginput, oldinput;
    struct ConfigParam cfg_param;
    struct RDArgs *rda;

    D(("config init:\n"));
    memset(&cfg_param, 0, sizeof(cfg_param));

    /* only need DOS for config */
    if(DOSBase = (struct DosLibrary *)OpenLibrary ("dos.library",36)) {
        /* try to read config */
        if(cfginput = Open(CONFIG_FILE, MODE_OLDFILE)) {
            D(("opened cfg file: " CONFIG_FILE "\n"));
            oldinput = SelectInput(cfginput);      
            rda = ReadArgs(ARG_TEMPLATE, (LONG *)&cfg_param, NULL);
            if(rda != NULL) {
                D(("got args\n"));
                parse_args(&cfg_param);
                FreeArgs(rda);
            } else {
                D(("failed parsing args!\n"));
            }
            Close(SelectInput(oldinput));
        } else {
            D(("cfg file not found: " CONFIG_FILE "\n"));
        }
        CloseLibrary((struct Library *)DOSBase);
    } else {
        D(("no DOS!\n"));
    }
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
            MidiMsg msg;
            msg.l[0] = pd->parser.msg;
            D(("cmd %08lx\n", msg.l[0]));

            int res = proto_send_msg(&proto, &msg);
            if(res != 0) {
                D(("send err: %ld\n", res));
            }
        }
        // send sysex
        else if(res == MIDI_PARSER_RET_SYSEX_OK) {
            MidiMsg msg;
            msg.mm_Status = MS_SysEx;
            D(("cmds %08lx\n", msg.l[0]));

            int res = proto_send_msg_buf(&proto, &msg, 
                pd->parser.sysex_buf, pd->parser.sysex_bytes);
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
            // is sysex?
            if(status == MS_SysEx) {
                ULONG num_bytes = 0;
                UBYTE *buf = NULL;
                proto_get_msg_buf(&proto, &buf, &num_bytes);
                for(ULONG i=0;i<num_bytes;i++) {
                    D(("rxs: %02lx\n", buf[i]));
                    pd->rx_func(buf[i], pd->user_data);
                }
            }
            // regular message
            else {
                int num_bytes = midi_parser_get_cmd_len(status);
                for(int i=0;i<num_bytes;i++) {
                    D(("rx: %02lx\n", msg.b[i]));
                    pd->rx_func(msg.b[i], pd->user_data);
                }
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

    if(proto_init(&proto, SysBase, sysex_max_size) != 0) {
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
    
    config_driver();

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
        if(midi_parser_init(&ports[portnum].parser, SysBase, (UBYTE)portnum,
                            sysex_max_size)!=0) {
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
