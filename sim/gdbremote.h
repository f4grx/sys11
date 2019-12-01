#ifndef __gdb__h__
#define __gdb__h__

#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

struct gdbremote_t
  {
    uint16_t port;
    int sock;
  };

int gdbremote_init(struct gdbremote_t *gr);
int gdbremote_close(struct gdbremote_t *gr);

#endif /* __gdb__h__ */

