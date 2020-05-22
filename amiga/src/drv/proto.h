#ifndef PROTO_H
#define PROTO_H

#include "udp.h"

struct proto_handle {
    struct ExecBase *sysBase;
    struct udp_handle udp;
    
    // config
    int server_port;
    int client_port;
    char *server_name;
    char *client_name;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    int udp_fd;

    UBYTE *buf;
    ULONG buf_max_bytes;
    ULONG buf_extra_bytes;
    ULONG tx_seq_num;
    ULONG rx_seq_num;
};

extern int proto_init(struct proto_handle *ph, struct ExecBase *sysBase, ULONG data_max_size);
extern void proto_exit(struct proto_handle *ph);

extern int proto_send_msg(struct proto_handle *ph, MidiMsg *msg);
extern int proto_send_msg_buf(struct proto_handle *ph, MidiMsg *msg, UBYTE *buf, ULONG size);
extern int proto_recv_msg(struct proto_handle *ph, MidiMsg *msg);
extern void proto_get_msg_buf(struct proto_handle *ph, UBYTE **buf, ULONG *size);
extern int proto_wait_recv(struct proto_handle *uh, ULONG timeout, ULONG *sigmask);

#endif
