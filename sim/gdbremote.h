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

#include "core.h"

#define GDBREMOTE_MAX_RX 1023
#define GDBREMOTE_MAX_TX 1023

struct gdbremote_t
  {
    uint16_t port;
    int sock;
    int client;
    sem_t startstop;
    bool running;
    pthread_t tid;
    char rxbuf[GDBREMOTE_MAX_RX + 1];
    char txbuf[GDBREMOTE_MAX_TX + 1];
    int rxlen,txlen;
    struct hc11_core *core;
    int waitack;
    int lastcommand; //flag to allow an async response when core was running then is stopped
  };

int gdbremote_init(struct gdbremote_t *gr);
int gdbremote_close(struct gdbremote_t *gr);
int gdbremote_stopped(struct gdbremote_t *gr);

#endif /* __gdb__h__ */

