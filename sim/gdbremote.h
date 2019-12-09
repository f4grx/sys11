#ifndef __gdb__h__
#define __gdb__h__

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <semaphore.h>

struct gdbremote_t
  {
    uint16_t port;
    int sock;
    sem_t startstop;
    bool running;
    pthread_t tid;
    char rxbuf[1024];
  };

int gdbremote_init(struct gdbremote_t *gr);
int gdbremote_close(struct gdbremote_t *gr);

#endif /* __gdb__h__ */

