#define __NOLIBBASE__
#include <proto/exec.h>
#include <libraries/bsdsocket.h>
#include <proto/bsdsocket.h>

#include "compiler.h"
#include "debug.h"
#include "udp.h"

#define SysBase uh->sysBase
#define SocketBase uh->socketBase

int udp_init(struct udp_handle *uh, struct ExecBase *sysBase)
{
    uh->sysBase = sysBase;
    uh->socketBase = OpenLibrary("bsdsocket.library", 0);
    if(uh->socketBase == NULL) {
        D(("no bsdsocket.library!"));
        return -1;
    }
    return 0;
}

void udp_exit(struct udp_handle *uh)
{
    if(uh->socketBase != NULL) {
        CloseLibrary(uh->socketBase);
        uh->socketBase = NULL;
    }
}

int udp_addr_setup(struct udp_handle *uh, struct sockaddr_in *addr,
                   char *name, UWORD port)
{
  /* resolve host */
  struct hostent *he;
  he = gethostbyname(name);
  if(he != NULL) {
    D(("resolved host\n"));

    addr->sin_family = AF_INET;      /* host byte order */
    addr->sin_port = htons(port);  /* short, network byte order */
    addr->sin_addr = *((struct in_addr *)he->h_addr);
    memset(&(addr->sin_zero), 0, 8);     /* zero the rest of the struct */
    return 0;
  }
  return -1;
}

int udp_open(struct udp_handle *uh, struct sockaddr_in *bind_addr)
{
  // open socket
  int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if(sock_fd < 0) {
    D(("socket failed: %d\n", sock_fd));
    return sock_fd;
  }

  // bind?
  if(bind_addr != NULL) {
    int result = bind(sock_fd, (struct sockaddr *)bind_addr, sizeof(struct sockaddr_in));
    if(result < 0) {
      D(("bind failed: %d\n", result));
      return result;
    }
  }

  return sock_fd;
}

void udp_close(struct udp_handle *uh, int sock_fd)
{
  CloseSocket(sock_fd);
}

int udp_send(struct udp_handle *uh, int sock_fd, struct sockaddr_in *peer_addr,
              void *buffer, ULONG len)
{
  int num = sendto(sock_fd, buffer, len, MSG_WAITALL,
                   (struct sockaddr *)peer_addr, sizeof(struct sockaddr_in));
  if(num < 0) {
    D(("sendto: failed %ld\n", num));
    return num;
  }

  if(num != len) {
    D(("sendto: wrong size %ld != %ld\n", num, len));
    return -1;
  }  

  return 0;
}

int udp_recv(struct udp_handle *uh, int sock_fd, struct sockaddr_in *ret_peer_addr,
              void *buffer, ULONG len)
{
  socklen_t addr_len;
  int num = recvfrom(sock_fd, buffer, len, 0,
                     (struct sockaddr *)ret_peer_addr, &addr_len);
  if(num < 0) {
    D(("recvfrom: failed %ld\n", num));
    return num;
  }

  if(addr_len != sizeof(struct sockaddr_in)) {
    D(("recvfrom: invalid addr len: %ld\n", addr_len));
    return -1;
  }

  return num;
}

int udp_wait_recv(struct udp_handle *uh, int sock_fd, ULONG timeout_us, ULONG *sigmask)
{
    struct timeval tv = { .tv_usec = timeout_us, .tv_sec = 0};
    struct timeval *tv_ptr = &tv;
    long n;
    fd_set read_fds;

    FD_ZERO(&read_fds);
    FD_SET(sock_fd, &read_fds);

    if(timeout_us == 0) {
      tv_ptr = NULL;
    }

    n = WaitSelect(sock_fd + 1, &read_fds, NULL, NULL, tv_ptr, sigmask);
    if(n==-1) {
        D(("WaitSelect: failed!\n"));
    }
    return n;
}
