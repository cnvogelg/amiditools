#define __NOLIBBASE__
#include <proto/exec.h>
#include <libraries/bsdsocket.h>
#include <midi/camd.h>

#include "compiler.h"
#include "debug.h"
#include "udp.h"
#include "midi-msg.h"
#include "proto.h"

#define SysBase ph->sysBase

int proto_init(struct proto_handle *ph, struct ExecBase *sysBase, ULONG data_max_size)
{
    ph->sysBase = sysBase;
    ph->rx_max_bytes = data_max_size;
    ULONG buf_size = data_max_size + sizeof(struct proto_packet); /* account for header */

    // allocate transfer buffer
    ph->rx_buf = AllocVec(buf_size, MEMF_PUBLIC);
    if(ph->rx_buf == NULL) {
        D(("proto: no mem!\n"));
        return PROTO_RET_ERROR_NO_MEM;
    }
    ph->tx_buf = AllocVec(buf_size, MEMF_PUBLIC);
    if(ph->tx_buf == NULL) {
        D(("proto: no mem!\n"));
        FreeVec(ph->rx_buf);
        return PROTO_RET_ERROR_NO_MEM;
    }

    // setup udp
    if(udp_init(&ph->udp, SysBase) != 0) {
        D(("proto: udp_init failed!\n"));
        FreeVec(ph->rx_buf);
        FreeVec(ph->tx_buf);
        return PROTO_RET_ERROR_UDP_INIT;
    }

    // resolve my name
    if(udp_addr_setup(&ph->udp, &ph->my_addr, ph->my_name, ph->my_port)!=0) {
        D(("proto: server name lookup failed: %s\n", ph->my_name));
        FreeVec(ph->rx_buf);
        FreeVec(ph->tx_buf);
        udp_exit(&ph->udp);
        return PROTO_RET_ERROR_UDP_ADDR;
    }

    // open my socket
    ph->udp_fd = udp_open(&ph->udp, &ph->my_addr);
    if(ph->udp_fd < 0) {
        D(("proto: error creating udp socket!\n"));
        FreeVec(ph->rx_buf);
        FreeVec(ph->tx_buf);
        udp_exit(&ph->udp);
        return PROTO_RET_ERROR_UDP_OPEN;
    }

    return PROTO_RET_OK;
}

void proto_exit(struct proto_handle *ph)
{
    if(ph->udp_fd >= 0) {
        D(("proto: close UDP socket\n"));
        udp_close(&ph->udp, ph->udp_fd);
    }

    D(("proto: UDP shutdown\n"));
    udp_exit(&ph->udp);

    FreeVec(ph->rx_buf);
    FreeVec(ph->tx_buf);
}

void proto_send_prepare(struct proto_handle *ph,
                        struct proto_packet **ret_pkt,
                        UBYTE **ret_data)
{
    *ret_pkt = (struct proto_packet *)ph->tx_buf;
    *ret_data = ph->tx_buf + sizeof(struct proto_packet);
}

int proto_send_packet(struct proto_handle *ph, 
                      struct sockaddr_in *peer_addr)
{
    struct proto_packet *pkt = (struct proto_packet *)ph->tx_buf;
    ULONG  data_size = pkt->data_size;
    ULONG  raw_size = sizeof(struct proto_packet) + data_size;

    if(!udp_send(&ph->udp, ph->udp_fd, peer_addr, ph->tx_buf, raw_size)) {
        return PROTO_RET_OK;
    } else {
        return PROTO_RET_ERROR_UDP_IO;
    }
}

int proto_recv_packet(struct proto_handle *ph,
                      struct sockaddr_in *peer_addr,
                      struct proto_packet **ret_pkt,
                      UBYTE **ret_data)
{
    int res = udp_recv(&ph->udp, ph->udp_fd, peer_addr, ph->rx_buf, ph->rx_max_bytes);
    if(res < sizeof(struct proto_packet)) {
        D(("proto: pkt too short!\n"));
        return PROTO_RET_ERROR_PKT_SHORT;
    }

    struct proto_packet *pkt = (struct proto_packet *)ph->rx_buf;

    // check magic
    ULONG magic = pkt->magic;
    if((magic & PROTO_MAGIC_MASK) != PROTO_MAGIC) {
        D(("proto: no magic!\n"));
        return PROTO_RET_ERROR_NO_MAGIC;
    }

    // check command
    UBYTE cmd = (UBYTE)(magic & 0xff);
    switch(cmd) {
        case PROTO_MAGIC_CMD_INV:
        case PROTO_MAGIC_CMD_EXIT:
        case PROTO_MAGIC_CMD_MIDI_MSG:
        case PROTO_MAGIC_CMD_MIDI_SYSEX:
        case PROTO_MAGIC_CMD_CLOCK:
            break;
        default:
            D(("proto: invalid cmd: %lx\n", cmd));
            return PROTO_RET_ERROR_WRONG_CMD;
    }

    // check data size
    ULONG pkt_data_size = res - sizeof(struct proto_packet);
    if(pkt_data_size != pkt->data_size) {
        D(("proto: wrong data size: want=%ld got=%ld\n", pkt_data_size, pkt->data_size));
        return PROTO_RET_ERROR_WRONG_SIZE;
    }

    // setip data
    *ret_pkt = pkt;
    *ret_data = ph->rx_buf + sizeof(struct proto_packet);

    return PROTO_RET_OK;
}

int proto_recv_wait(struct proto_handle *ph, ULONG timeout_s, ULONG timeout_us, ULONG *sigmask)
{
    return udp_wait_recv(&ph->udp, ph->udp_fd, timeout_s, timeout_us, sigmask);
}
