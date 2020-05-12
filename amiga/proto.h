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
    ULONG tx_seq_num;
    ULONG rx_seq_num;
};

extern int proto_init(struct proto_handle *ph, struct ExecBase *sysBase);
extern void proto_exit(struct proto_handle *ph);

extern int proto_send_msg(struct proto_handle *ph, MidiMsg *msg);
extern int proto_recv_msg(struct proto_handle *ph, MidiMsg *msg);
extern int proto_wait_recv(struct proto_handle *uh, ULONG timeout, ULONG *sigmask);

#endif
