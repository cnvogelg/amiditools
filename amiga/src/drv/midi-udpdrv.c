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
static char server_name[NAME_LEN] = "localhost";
static char client_name[NAME_LEN] = "localhost";
static struct proto_handle proto = {
    .server_name = server_name,
    .client_name = client_name,
    .server_port = 6820,
    .client_port = 6821
};

/* Config Driver */

#define CONFIG_FILE "ENV:midi/udp.config"
#define ARG_TEMPLATE \
    "CLIENT_HOST/K,CLIENT_PORT/K/N," \
    "SERVER_HOST/K,SERVER_PORT/K/N," \
    "SYSEX_SIZE/K/N"
struct midi_drv_config_param {
    STRPTR client_host;
    ULONG *client_port;
    STRPTR server_host;
    ULONG *server_port;
    ULONG *sysex_size;
};

static int parse_args(struct midi_drv_config_param *param)
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
        midi_drv_sysex_max_size = *param->sysex_size;
    }
    return MIDI_DRV_RET_OK;
}

void midi_drv_api_config(void)
{
    struct midi_drv_config_param param = { NULL, NULL, NULL, NULL, NULL };
    midi_drv_config(CONFIG_FILE, ARG_TEMPLATE, &param, parse_args);
}

void midi_drv_api_tx_msg(int port, midi_msg_t msg)
{
    int res = proto_send_msg(&proto, port, msg);
    if(res != 0) {
        D(("proto: msg send err: %ld\n", res));
    }
}

void midi_drv_api_tx_sysex(int port, midi_msg_t msg, UBYTE *data, ULONG size)
{
    int res = proto_send_sysex(&proto, port, msg, data, size);
    if(res != 0) {
        D(("proto: sysex send err: %ld\n", res));
    }
}

int midi_drv_api_rx_msg(int *port, midi_msg_t *msg)
{
    int res = proto_recv_msg(&proto, port, msg);
    if(res == 0) {
        return MIDI_DRV_RET_OK;
    } else {
        return MIDI_DRV_RET_IO_ERROR;
    }
}

void midi_drv_api_rx_sysex(int port, UBYTE **data, ULONG *size)
{
    proto_get_msg_buf(&proto, data, size);
}

int midi_drv_api_rx_wait(ULONG *got_mask)
{
    int res = proto_wait_recv(&proto, 0, got_mask);
    if(res == -1) {
        return MIDI_DRV_RET_IO_ERROR;
    }
    else if(res == 0) {
        return MIDI_DRV_RET_SIGNAL;
    }
    else {
        return MIDI_DRV_RET_OK;
    }
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
