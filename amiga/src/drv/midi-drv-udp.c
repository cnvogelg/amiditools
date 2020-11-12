/*
 * a CAMD midi driver for classic Amigas
 */

#include <proto/exec.h>
#include <proto/alib.h>
#include <proto/timer.h>
#include <libraries/bsdsocket.h>
#include <midi/camddevices.h>
#include <midi/camd.h>
#include <midi/mididefs.h>

#include "debug.h"
#include "compiler.h"
#include "midi-msg.h"
#include "midi-drv.h"
#include "udp.h"
#include "proto.h"

// functions
static void timer_set(ULONG secs, ULONG micro);
static void timer_abort(void);

/* midi driver structure */
static struct MidiDeviceData my_dev = {
    .Magic = MDD_Magic,
    .Name = "udp",
    .IDString = "UDP network midi",
    .Version = 0,
    .Revision = 1,
    .Init = (APTR)midi_drv_init,
    .Expunge = midi_drv_expunge,
    .OpenPort = midi_drv_open_port,
    .ClosePort = midi_drv_close_port,
    .NPorts = MIDI_DRV_NUM_PORTS,
    .Flags = 1, // new style driver
};

extern struct ExecBase *SysBase;
struct Device *TimerBase;
static struct timerequest *ior_time;
static ULONG timer_mask;
static struct MsgPort *timer_port;

// UDP config
#define NAME_LEN 80
static char host_name[NAME_LEN] = "0.0.0.0";
static struct proto_handle proto = {
    .my_name = host_name,
    .my_port = 6820
};

// peer state
static int peer_connected;
static ULONG peer_tx_seq_num;
static ULONG peer_rx_seq_num;
static struct sockaddr_in peer_addr;
static int clock_interval = 5;
static int peer_data_received = FALSE;

/* Config Driver */

#define CONFIG_FILE "ENV:midi/udp.config"
#define ARG_TEMPLATE \
    "HOST_NAME/K,PORT/K/N," \
    "SYSEX_SIZE/K/N"
struct midi_drv_config_param {
    STRPTR host_name;
    ULONG *port;
    ULONG *sysex_size;
};

static int parse_args(struct midi_drv_config_param *param)
{
    if(param->host_name != NULL) {
        D(("set host name: %s\n", param->host_name));
        strncpy(host_name, param->host_name, NAME_LEN);
    }
    if(param->port != NULL) {
        D(("set port: %ld\n", *param->port));
        proto.my_port = *param->port;
    }
    if(param->sysex_size != NULL) {
        D(("set sysex size: %ld\n", *param->sysex_size));
        midi_drv_sysex_max_size = *param->sysex_size;
    }
    return MIDI_DRV_RET_OK;
}

STRPTR midi_drv_api_config(void)
{
    struct midi_drv_config_param param = { NULL, NULL, NULL };
    midi_drv_config(CONFIG_FILE, ARG_TEMPLATE, &param, parse_args);
    return "midi.udp";
}

void midi_drv_api_tx_msg(midi_drv_msg_t *msg)
{
    struct proto_packet *pkt;
    UBYTE *data_buf;

    if(!peer_connected) {
        D(("midi-udp: tx: no peer!\n"));
        return;
    }

    proto_send_prepare(&proto, &pkt, &data_buf);

    pkt->port = msg->port;
    pkt->seq_num = ++peer_tx_seq_num;
    GetSysTime(&pkt->time_stamp);

    ULONG sysex_size = msg->sysex_size;
    if(sysex_size > 0) {
        pkt->magic = PROTO_MAGIC | PROTO_MAGIC_CMD_MIDI_SYSEX;
        CopyMem(msg->sysex_data, data_buf, sysex_size);
        pkt->data_size = sysex_size;
    } else {
        pkt->magic = PROTO_MAGIC | PROTO_MAGIC_CMD_MIDI_MSG;
        midi_msg_t *ptr = (midi_msg_t *)data_buf;
        *ptr = msg->midi_msg;
        pkt->data_size = sizeof(midi_msg_t);
    }

    int res = proto_send_packet(&proto, &peer_addr);
    if(res != 0) {
        D(("midi-udp: tx err: %ld\n", res));
    } else {
        D(("midi-udp: tx OK\n"));
    }
}

static void handle_timer(void)
{
    if(!peer_connected) {
        D(("peer: already disconnected!\n"));
        return;
    }
    if(!peer_data_received) {
        D(("peer: auto-disconnect!\n"));
        peer_connected = FALSE;
        peer_data_received = FALSE;
    } else {
        D(("peer: alive.\n"))
        peer_data_received = FALSE;
        timer_set(clock_interval, 0);
    }
}

#define check_peer_addr(addr) \
    (addr->sin_addr.s_addr == peer_addr.sin_addr.s_addr)

static void handle_peer_invitation(struct sockaddr_in *this_peer_addr,
                                   struct proto_packet *pkt)
{
    // prepare response
    UBYTE cmd = peer_connected ? PROTO_MAGIC_CMD_INV_NO : PROTO_MAGIC_CMD_INV_OK;
    D(("midi-udp: inv: reply=%02lx\n", cmd));

    // reply
    struct proto_packet *ret_pkt;
    UBYTE *ret_data_buf;
    proto_send_prepare(&proto, &ret_pkt, &ret_data_buf);

    ret_pkt->magic = PROTO_MAGIC | cmd;
    ret_pkt->port = pkt->port;
    GetSysTime(&ret_pkt->time_stamp);
    ret_pkt->seq_num = pkt->time_stamp.tv_micro;
    ret_pkt->data_size = 0;

    int res = proto_send_packet(&proto, this_peer_addr);
    if(res != 0) {
        D(("midi-udp: tx err: %ld\n", res));
    }

    // accept client
    if(!peer_connected) {
        peer_connected = TRUE;
        peer_rx_seq_num = pkt->seq_num;
        peer_tx_seq_num = ret_pkt->seq_num;
        peer_addr = *this_peer_addr;
        D(("midi-udp: connected: rx_seq=%08lx, tx_seq=%08lx\n",
            peer_rx_seq_num, peer_tx_seq_num));
        // fire timer
        timer_set(clock_interval, 0);
        peer_data_received = FALSE;
    }
}

static void handle_peer_exit(struct sockaddr_in *this_peer_addr,
                             struct proto_packet *pkt)
{
    // check addr
    if(!check_peer_addr(this_peer_addr)) {
        D(("midi-udp: exit: peer wrong addr!\n"));
        return;
    }

    if(!peer_connected) {
        D(("midi-udp: exit: not connected!\n"));
        return;
    }

    peer_connected = FALSE;
    timer_abort();
    D(("midi-udp: disconnected!\n"));
}

static void handle_peer_clock(struct sockaddr_in *this_peer_addr,
                              struct proto_packet *pkt, UBYTE *data_buf)
{
    // reply
    struct proto_packet *ret_pkt;
    UBYTE *ret_data_buf;
    proto_send_prepare(&proto, &ret_pkt, &ret_data_buf);

    ret_pkt->magic = PROTO_MAGIC | PROTO_MAGIC_CMD_CLOCK;
    ret_pkt->port = pkt->port;
    GetSysTime(&ret_pkt->time_stamp);
    ret_pkt->seq_num = ++peer_tx_seq_num;
    ret_pkt->data_size = 0;

    int res = proto_send_packet(&proto, this_peer_addr);
    if(res != 0) {
        D(("midi-udp: tx err: %ld\n", res));
    }

    D(("peer clock: %ld, %ld\n", pkt->time_stamp.tv_secs, pkt->time_stamp.tv_micro));
    D(("send clock: %ld, %ld\n", ret_pkt->time_stamp.tv_secs, ret_pkt->time_stamp.tv_micro));
}

static midi_drv_msg_t my_msg;

static midi_drv_msg_t *handle_peer_midi_msg(struct sockaddr_in *this_peer_addr, 
                                            struct proto_packet *pkt, UBYTE *data_buf)
{
    // check addr
    if(!check_peer_addr(this_peer_addr)) {
        D(("midi-udp: midi_msg: peer wrong addr!\n"));
        return NULL;
    }

    // check size
    if(pkt->data_size != sizeof(midi_msg_t)) {
        D(("midi.udp: midi_msg: wrong size!\n"));
        return NULL;
    }

    my_msg.port = pkt->port;
    my_msg.midi_msg = *((midi_msg_t *)data_buf);
    my_msg.sysex_data = NULL;
    my_msg.sysex_size = 0;
    return &my_msg;
}

static midi_drv_msg_t *handle_peer_midi_sysex(struct sockaddr_in *this_peer_addr, 
                                              struct proto_packet *pkt, UBYTE *data_buf)
{
    // check addr
    if(!check_peer_addr(this_peer_addr)) {
        D(("midi-udp: midi sysex: peer wrong addr!\n"));
        return NULL;
    }

    // check size
    if(pkt->data_size < 3) {
        D(("midi.udp: midi sysex: wrong size!\n"));
        return NULL;
    }

    my_msg.port = pkt->port;
    my_msg.midi_msg = *((midi_msg_t *)data_buf);
    my_msg.sysex_data = data_buf;
    my_msg.sysex_size = pkt->data_size;
    return &my_msg;
}

int midi_drv_api_rx_msg(midi_drv_msg_t **msg, ULONG *got_mask)
{
    // loop until a midi message was received or a signal occurred
    // stay in the loop when other protocol messages appear
    ULONG start_mask = *got_mask;
    while(1) {
        ULONG my_mask = start_mask | timer_mask;
        D(("midi-udp: rx wait: mask=%08lx\n", my_mask));
        int res = proto_recv_wait(&proto, 0, 0, &my_mask);
        D(("midi-udp: rx -> %ld, mask=%08lx\n", res, my_mask));
        *got_mask = my_mask;
        if(res == -1) {
            return MIDI_DRV_RET_IO_ERROR;
        }

        // timer?
        if((my_mask & timer_mask) == timer_mask) {
            GetMsg(timer_port);
            handle_timer();
        }

        // got data?
        if(res > 0) {
            struct proto_packet *pkt;
            UBYTE *data_buf;
            static struct sockaddr_in pkt_peer_addr;
            int res = proto_recv_packet(&proto, &pkt_peer_addr, &pkt, &data_buf);
            if(res == 0) {
                peer_data_received = TRUE;
                // get packet type
                UBYTE cmd = (UBYTE)(pkt->magic & PROTO_MAGIC_CMD_MASK);
                D(("midi-udp: rx pkt: cmd=%02lx port=%ld seq_num=%08lx ts=%ld.%06ld size=%ld\n",
                    cmd, pkt->port, pkt->seq_num,
                    pkt->time_stamp.tv_secs, pkt->time_stamp.tv_micro,
                    pkt->data_size));
                D(("peer: port=%ld addr=%08lx\n",
                    pkt_peer_addr.sin_port,
                    pkt_peer_addr.sin_addr));
                switch(cmd) {
                    case PROTO_MAGIC_CMD_INV:
                        handle_peer_invitation(&pkt_peer_addr, pkt);
                        break;
                    case PROTO_MAGIC_CMD_EXIT:
                        handle_peer_exit(&pkt_peer_addr, pkt);
                        break;
                    case PROTO_MAGIC_CMD_CLOCK:
                        handle_peer_clock(&pkt_peer_addr, pkt, data_buf);
                        break;
                    case PROTO_MAGIC_CMD_MIDI_MSG:
                        {
                            midi_drv_msg_t *my_msg = handle_peer_midi_msg(
                                &pkt_peer_addr, pkt, data_buf);
                            if(my_msg != NULL) {
                                *msg = my_msg;
                                return MIDI_DRV_RET_OK;
                            }
                        }
                        break;
                    case PROTO_MAGIC_CMD_MIDI_SYSEX:
                        {
                            midi_drv_msg_t *my_msg = handle_peer_midi_sysex(
                                &pkt_peer_addr, pkt, data_buf);
                            if(my_msg != NULL) {
                                *msg = my_msg;
                                return MIDI_DRV_RET_OK;
                            }
                        }
                        break;
                    default:
                        D(("ERROR invalid cmd!\n"));
                        break;
                }
            } else {
                return MIDI_DRV_RET_IO_ERROR;
            }
        }

        // more signals? leave loop
        if((my_mask & start_mask) != 0) {
            D(("midi-udp: rx -> other signal: %lx\n", my_mask));
            return MIDI_DRV_RET_OK;
        }

    }
    return MIDI_DRV_RET_OK;
}

void midi_drv_api_rx_msg_done(midi_drv_msg_t *msg)
{
    // nothing to do
}

static int timer_init(void)
{
    struct MsgPort *port;
    LONG error;

    port = CreatePort(NULL, 0);
    if(port == NULL) {
        return 1;
    }

    ior_time = (struct timerequest *)CreateExtIO(port, sizeof(struct timerequest));
    if(ior_time == NULL) {
        DeletePort(port);
        return 2;
    }

    error = OpenDevice(TIMERNAME, UNIT_MICROHZ, (struct IORequest *)ior_time, 0L);
    if(error != 0) {
        DeleteExtIO((struct IORequest *)ior_time);
        DeletePort(port);
        return 3;

    }

    TimerBase = ior_time->tr_node.io_Device;
    timer_mask = 1 << port->mp_SigBit;
    timer_port = port;
    return 0;
}

static void timer_exit(void)
{
    struct MsgPort *port;

    if(ior_time == NULL) {
        return;
    }

    port = timer_port;

    AbortIO((struct IORequest *)ior_time);
    WaitIO((struct IORequest *)ior_time);

    CloseDevice((struct IORequest *)ior_time);
    DeleteExtIO((struct IORequest *)ior_time);
    DeletePort(port);

    TimerBase = NULL;
    timer_port = NULL;
    timer_mask = 0;
    ior_time = NULL;
}

static void timer_set(ULONG secs, ULONG micro)
{
    ior_time->tr_node.io_Command = TR_ADDREQUEST;
    ior_time->tr_time.tv_secs = secs;
    ior_time->tr_time.tv_micro = micro;

    SendIO((struct IORequest *)ior_time);
    D(("timer: set secs=%ld micro=%ld mask=%08lx\n", secs, micro, timer_mask));
}

static void timer_abort(void)
{
    D(("timer: abort\n"));
    AbortIO((struct IORequest *)ior_time);
    WaitIO((struct IORequest *)ior_time);
}

int midi_drv_api_init(struct ExecBase *SysBase)
{
    // timer init
    int error = timer_init();
    if(error != 0) {
        D(("midi-udp: timer init fauled!\n"));
        return MIDI_DRV_RET_FATAL_ERROR;
    }

    // proto init
    if(proto_init(&proto, SysBase, midi_drv_sysex_max_size) != 0) {
        D(("midi: proto_init failed!\n"));
        timer_exit();
        return MIDI_DRV_RET_FATAL_ERROR;
    }

    return MIDI_DRV_RET_OK;
}

void midi_drv_api_exit(void)
{
    timer_exit();
    proto_exit(&proto);
}
