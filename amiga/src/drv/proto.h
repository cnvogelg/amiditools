#ifndef PROTO_H
#define PROTO_H

#include "udp.h"

#define PROTO_RET_OK                0
#define PROTO_RET_ERROR_NO_MEM      1
#define PROTO_RET_ERROR_UDP_INIT    2
#define PROTO_RET_ERROR_UDP_ADDR    3
#define PROTO_RET_ERROR_UDP_OPEN    4
#define PROTO_RET_ERROR_UDP_IO      5
#define PROTO_RET_ERROR_PKT_SHORT   6
#define PROTO_RET_ERROR_NO_MAGIC    7
#define PROTO_RET_ERROR_PKT_LARGE   8
#define PROTO_RET_ERROR_WRONG_CMD   9
#define PROTO_RET_ERROR_WRONG_SIZE  10

struct proto_handle {
    struct ExecBase *sysBase;
    struct udp_handle udp;
    
    // config
    int my_port;
    char *my_name;

    // my socket
    struct sockaddr_in my_addr;
    int udp_fd;

    UBYTE *tx_buf;
    UBYTE *rx_buf;
    ULONG rx_max_bytes;
};

#define PROTO_DATA_SIZE     4

struct proto_packet {
    ULONG  magic;
    ULONG  port;
    ULONG  seq_num;
    struct timeval  time_stamp;
    ULONG  data_size;
};

#define PROTO_MAGIC           0x43414d00     // CAMx
#define PROTO_MAGIC_MASK      0xffffff00
#define PROTO_MAGIC_CMD_INV       0x49 // 'I'
#define PROTO_MAGIC_CMD_INV_OK    0x4f // 'O'
#define PROTO_MAGIC_CMD_INV_NO    0x4e // 'N'
#define PROTO_MAGIC_CMD_EXIT      0x45 // 'E'
#define PROTO_MAGIC_CMD_DATA      0x44 // 'D'
#define PROTO_MAGIC_CMD_CLOCK     0x43 // 'C'

extern int proto_init(struct proto_handle *ph, struct ExecBase *sysBase, ULONG data_max_size);
extern void proto_exit(struct proto_handle *ph);

extern void proto_send_prepare(struct proto_handle *ph,
                               struct proto_packet **ret_pkt,
                               UBYTE **ret_data);

extern int proto_send_packet(struct proto_handle *ph,
                             struct sockaddr_in *peer_addr);

extern int proto_recv_packet(struct proto_handle *ph,
                             struct sockaddr_in *peer_addr,
                             struct proto_packet **ret_pkt,
                             UBYTE **ret_data);

extern int proto_recv_wait(struct proto_handle *uh, 
                           ULONG timeout_s, ULONG timeout_us, 
                           ULONG *sigmask);

#endif
