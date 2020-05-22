#define __NOLIBBASE__
#include <proto/exec.h>
#include <libraries/bsdsocket.h>
#include <midi/camd.h>

#include "compiler.h"
#include "debug.h"
#include "udp.h"
#include "proto.h"

#define SysBase ph->sysBase

#define MAGIC    0x43414d44     // CAMD

int proto_init(struct proto_handle *ph, struct ExecBase *sysBase, ULONG data_max_size)
{
    ph->sysBase = sysBase;
    ph->buf_max_bytes = data_max_size + 20; /* account for header */
    ph->tx_seq_num = 1;
    ph->rx_seq_num = 0;

    // allocate transfer buffer
    ph->buf = AllocVec(ph->buf_max_bytes, MEMF_PUBLIC);
    if(ph->buf == NULL) {
        D(("proto: no mem!\n"));
        return 5;
    }

    // setup udp
    if(udp_init(&ph->udp, SysBase) != 0) {
        D(("proto: udp_init failed!\n"));
        return 1;
    }

    // resolve names
    if(udp_addr_setup(&ph->udp, &ph->server_addr, ph->server_name, ph->server_port)!=0) {
        D(("proto: server name lookup failed: %s\n", ph->server_name));
        udp_exit(&ph->udp);
        return 2;
    }
   if(udp_addr_setup(&ph->udp, &ph->client_addr, ph->client_name, ph->client_port)!=0) {
        D(("proto: client name lookup failed: %s\n", ph->client_name));
        udp_exit(&ph->udp);
        return 3;
    }

    // open sockets
    ph->udp_fd = udp_open(&ph->udp, &ph->client_addr);
    if(ph->udp_fd < 0) {
        D(("proto: error creating udp socket!\n"));
        udp_exit(&ph->udp);
        return 4;
    }

    return 0;
}

void proto_exit(struct proto_handle *ph)
{
    if(ph->udp_fd >= 0) {
        D(("proto: close UDP socket\n"));
        udp_close(&ph->udp, ph->udp_fd);
    }

    D(("proto: UDP shutdown\n"));
    udp_exit(&ph->udp);

    FreeVec(ph->buf);
}

int proto_send_msg(struct proto_handle *ph, MidiMsg *msg)
{
    ULONG *buf = (ULONG *)ph->buf;
    buf[0] = MAGIC;
    buf[1] = ph->tx_seq_num++;
    buf[2] = msg->l[0];
    buf[3] = msg->l[1];

    return udp_send(&ph->udp, ph->udp_fd, &ph->server_addr, buf, 16);
}

int proto_send_msg_buf(struct proto_handle *ph, MidiMsg *msg, UBYTE *src_buf, ULONG size)
{
    ULONG *buf = (ULONG *)ph->buf;
    buf[0] = MAGIC;
    buf[1] = ph->tx_seq_num++;
    buf[2] = msg->l[0];
    buf[3] = msg->l[1];

    memcpy(&buf[4], src_buf, size);

    return udp_send(&ph->udp, ph->udp_fd, &ph->server_addr, buf, 20 + size);
}

int proto_recv_msg(struct proto_handle *ph, MidiMsg *msg)
{
    struct sockaddr_in client_addr;
    int res = udp_recv(&ph->udp, ph->udp_fd, &client_addr, ph->buf, ph->buf_max_bytes);
    if(res < 16) {
        return 1;
    }

    ULONG *buf = (ULONG *)ph->buf;
    // check magic
    if(buf[0] != MAGIC) {
        D(("proto: no magic!\n"));
        return 2;
    }

    // check sequence number
    ph->rx_seq_num++;
    if(ph->rx_seq_num != buf[1]) {
        D(("proto: rx_seq_num mismatch: %ld != %ld", ph->rx_seq_num, buf[1]));
    }

    // retrieve midi msg
    msg->l[0] = buf[2];
    msg->l[1] = buf[3];

    // store extra size    
    if(res >= 16) {
        ph->buf_extra_bytes = res - 16;
    } else {
        ph->buf_extra_bytes = 0;
    }

    return 0;
}

void proto_get_msg_buf(struct proto_handle *ph, UBYTE **buf, ULONG *size)
{
    *size = ph->buf_extra_bytes;
    *buf = ph->buf + 16;
}

int proto_wait_recv(struct proto_handle *ph, ULONG timeout, ULONG *sigmask)
{
    return udp_wait_recv(&ph->udp, ph->udp_fd, timeout, sigmask);
}
