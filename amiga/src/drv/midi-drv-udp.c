/*
 * a CAMD midi driver for classic Amigas
 */

#include <proto/exec.h>
#include <proto/alib.h>
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

// UDP config
#define NAME_LEN 80
static char host_name[NAME_LEN] = "0.0.0.0";
static struct proto_handle proto = {
    .my_name = host_name,
    .my_port = 6820
};
static struct sockaddr_in peer_addr;

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

    proto_send_prepare(&proto, &pkt, &data_buf);

    pkt->magic = PROTO_MAGIC | PROTO_MAGIC_CMD_DATA;
    pkt->port = msg->port;
    pkt->seq_num = 42;
    pkt->time_stamp.tv_secs = 4711;
    pkt->time_stamp.tv_micro = 112;

    ULONG sysex_size = msg->sysex_size;
    if(sysex_size > 0) {
        CopyMem(msg->sysex_data, data_buf, sysex_size);
        pkt->data_size = sysex_size;
    } else {
        midi_msg_t *ptr = (midi_msg_t *)data_buf;
        *ptr = msg->midi_msg;
        pkt->data_size = sizeof(midi_msg_t);
    }

    int res = proto_send_packet(&proto, &peer_addr);
    if(res != 0) {
        D(("proto: msg send err: %ld\n", res));
    }
}

static midi_drv_msg_t my_msg;

int midi_drv_api_rx_msg(midi_drv_msg_t **msg, ULONG *got_mask)
{
    D(("proto: rx\n"));
    int res = proto_recv_wait(&proto, 0, 0, got_mask);
    D(("proto: rx -> %ld\n", res));
    if(res == -1) {
        return MIDI_DRV_RET_IO_ERROR;
    }
    else if(res == 0) {
        return MIDI_DRV_RET_OK_SIGNAL;
    }
    else {
        struct proto_packet *pkt;
        UBYTE *data_buf;
        struct sockaddr_in peer_addr;
        int res = proto_recv_packet(&proto, &peer_addr, &pkt, &data_buf);
        if(res == 0) {
            *msg = &my_msg;
            my_msg.port = pkt->port;
            my_msg.midi_msg = *((midi_msg_t *)data_buf);

            return MIDI_DRV_RET_OK;
        } else {
            return MIDI_DRV_RET_IO_ERROR;
        }
    }
}

void midi_drv_api_rx_msg_done(midi_drv_msg_t *msg)
{
    // nothing to do
}

int midi_drv_api_init(struct ExecBase *SysBase)
{
    if(proto_init(&proto, SysBase, midi_drv_sysex_max_size) != 0) {
        D(("midi: proto_init failed!\n"));
        return MIDI_DRV_RET_FATAL_ERROR;
    } else {
        return MIDI_DRV_RET_OK;
    }
}

void midi_drv_api_exit(void)
{
    proto_exit(&proto);
}
