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

#define GDBREMOTE_STOP_NORMAL 0x02
#define GDBREMOTE_STOP_FAIL   0x05

struct gdbremote_t
  {
    uint16_t port;
    int sock;
    int client;
    sem_t startstop;
    bool running;
    bool connected;
    pthread_t tid;
    int rxlen,txlen;
    struct hc11_core *core;
    int lastcommand; //flag to allow an async response when core was running then is stopped
    char rxbuf[GDBREMOTE_MAX_RX + 1];
    char txbuf[GDBREMOTE_MAX_TX + 1];
  };

int gdbremote_init(struct gdbremote_t *gr);
int gdbremote_close(struct gdbremote_t *gr);
int gdbremote_stopped(struct gdbremote_t *gr, uint8_t reason);

#endif /* __gdb__h__ */

