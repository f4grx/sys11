#define _GNU_SOURCE

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "core.h"
#include "log.h"

enum
  {
    OFF_BAUD,
    OFF_SCCR1,
    OFF_SCCR2,
    OFF_SCSR,
    OFF_SCDR,
    REGCNT,
    SCI_REG_FIRST = 0x2B
  };

#define SCSR_RDRF 0x20
#define SCSR_TC   0x40
#define SCSR_TDRE 0x80

struct hc11_sci
  {
    uint8_t regs[REGCNT];
    int port;
    int sock;
    int client;
    pthread_t thread;
    sem_t startstop;
    bool running;
    bool connected;
    uint8_t txbuf;
  };

static uint8_t sci_read(void *ctx, uint16_t off)
  {
    struct hc11_sci *sci = ctx;
    uint8_t ret;
    off -= SCI_REG_FIRST;
    ret = sci->regs[off];
    switch(off)
      {
      case OFF_BAUD:  log_msg(SYS_SCI, 0, "SCI read BAUD -> %02X\n" , ret);  break;
      case OFF_SCCR1: log_msg(SYS_SCI, 0, "SCI read SCCR1 -> %02X\n", ret); break;
      case OFF_SCCR2: log_msg(SYS_SCI, 0, "SCI read SCCR2 -> %02X\n", ret); break;
      case OFF_SCSR:  log_msg(SYS_SCI, 0, "SCI read SCSR -> %02X\n" , ret);  break;
      case OFF_SCDR:
        sci->regs[OFF_SCSR] &= ~SCSR_RDRF;
        log_msg(SYS_SCI, 0, "SCI read SCDR -> %02X\n", ret);
        break;
      }
    return ret;
  }

static void sci_write(void *ctx, uint16_t off, uint8_t val)
  {
    struct hc11_sci *sci = ctx;
    off -= SCI_REG_FIRST;
    sci->regs[off] = val;
    switch(off)
      {
      case OFF_BAUD:  log_msg(SYS_SCI, 0, "SCI write BAUD <- %02X\n" , val); break;
      case OFF_SCCR1: log_msg(SYS_SCI, 0, "SCI write SCCR1 <- %02X\n", val); break;
      case OFF_SCCR2: log_msg(SYS_SCI, 0, "SCI write SCCR2 <- %02X\n", val); break;
      case OFF_SCSR:  log_msg(SYS_SCI, 0, "SCI write SCSR <- %02X\n" , val); break;
      case OFF_SCDR:
        sci->regs[OFF_SCSR] &= ~SCSR_TC;
        sci->regs[OFF_SCSR] &= ~SCSR_TDRE;
        log_msg(SYS_SCI, 0, "SCI write SCDR <- %02X\n", val);
        break;
      }
  }

static void sci_thread_sig(int num, siginfo_t *info, void *v)
  {
    struct hc11_sci *sci = info->si_value.sival_ptr;
    log_msg(SYS_SCI, 0, "hc11_sci: thread signal caught\n");
    sci->connected = false;
    sci->running   = false;
  }

static void* sci_thread(void *param)
  {
    struct hc11_sci *sci = param;
    struct sockaddr_in client;
    struct sigaction sa_thread;
    int ret;
    char buf;

    memset(&sa_thread, 0, sizeof(struct sigaction));
    sa_thread.sa_sigaction = sci_thread_sig;
    sa_thread.sa_flags     = SA_SIGINFO;
    //sigaction(SIGUSR1, &sa_thread, NULL);

    sci->running = true;
    sci->connected = false;
    log_msg(SYS_SCI, 0, "hc11_sci: listen thread start (port %u)\n", sci->port);
    sem_post(&sci->startstop);

    while(sci->running)
      {
        int clientsize = sizeof(client);
        int flags;
        sci->client = accept(sci->sock, (struct sockaddr*)&client, &clientsize);
        if(sci->client < 0)
          {
            perror("sci_accept()");
            break;
          }
        log_msg(SYS_SCI, 0, "hc11_sci: client connected\n");
        flags = fcntl(sci->client, F_GETFL, 0);
        if(flags == -1)
          {
           perror("fcntl GETFL");
          }
        flags |= O_NONBLOCK;
        ret = fcntl(sci->client, F_SETFL, flags);
        if(ret != 0)
          {
            perror("fcntl SETFL");
          }
        dprintf(sci->client, "SCI monitor\r\n");
        sci->connected = true;
        while(sci->connected)
          {
            if(!(sci->regs[OFF_SCSR] & SCSR_TDRE))
              {
                buf = sci->regs[OFF_SCDR];
                log_msg(SYS_SCI, 0, "hc11_sci: transmit %02X\n", (int)(buf&0xFF));
                ret = send(sci->client, &buf, 1, 0);
                sci->regs[OFF_SCSR] |= SCSR_TC;
                sci->regs[OFF_SCSR] |= SCSR_TDRE;
                if(ret != 1)
                  {
                    log_msg(SYS_SCI, 0, "hc11_sci: warning: transmit failed\n");
                  }
              }

            ret = recv(sci->client, &buf, 1, 0);
            if(ret == 0)
              {
//                if(errno != EAGAIN)
//                  {
                    log_msg(SYS_SCI, 0, "zero ret\n");
                    sci->connected = false;
//                  }
              }
            else if(ret < 0)
              {
                if(errno != EAGAIN)
                  {
                    log_msg(SYS_SCI, 0, "neg ret\n");
                    sci->connected = false;
                  }
              }
            else if(ret == 1)
              {
                if(!(sci->regs[OFF_SCSR] & SCSR_RDRF))
                  {
                    log_msg(SYS_SCI, 0, "sci: received a char %02X\n", (int)(buf&0xFF));
                    sci->regs[OFF_SCDR] = buf;
                    sci->regs[OFF_SCSR] |= SCSR_RDRF;
                  }
               else
                 {
                   log_msg(SYS_SCI, 0, "sci: warning: RX register already full, lost byte %02X\n", (int)(buf&0xFF));
                 }
              }
          }
        sci->connected = false;
        log_msg(SYS_SCI, 0, "hc11_sci: connection closed\n");
        close(sci->client);
      }

    log_msg(SYS_SCI, 0, "hc11_sci: listen thread done\n");
    return NULL;
  }

struct hc11_sci* hc11_sci_init(struct hc11_core *core)
  {
    struct hc11_sci *sci;
    struct sockaddr_in server;
    int ret;
    int yes = 1;

    log_msg(SYS_SCI, 0, "hc11_sci: starting\n");

    sci = malloc(sizeof(struct hc11_sci));
    if(!sci)
      {
        return NULL;
      }

    memset(sci->regs, 0, 5);
    sci->regs[OFF_SCSR] = SCSR_TDRE; //transmit buf is initially empty
    hc11_core_iocallback(core, SCI_REG_FIRST, 5, sci, sci_read, sci_write);

    sem_init(&sci->startstop, 0, 0);
    sci->port = 3334;

    // create tcp socket to allow gdb incoming connection
    sci->sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sci->sock < 0)
      {
        perror("socket");
        goto release;
      }

    ret = setsockopt(sci->sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    if(ret < 0)
      {
        perror("setsockopt SO_REUSEADDR");
        goto close;
      }


    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = ntohs(sci->port);
    ret = bind(sci->sock, (struct sockaddr*)&server, sizeof(server));
    if(ret < 0)
      {
        perror("bind()");
        goto close;
      }

    ret = listen(sci->sock, 0);
    if(ret < 0)
      {
        perror("listen()");
        goto close;
      }

    pthread_create(&sci->thread, NULL, sci_thread, sci);
    sem_wait(&sci->startstop);
    log_msg(SYS_SCI, 0, "hc11_sci: started\n");
    return sci;

release:
    free(sci);
close:
    close(sci->sock);
    return NULL;
  }

int hc11_sci_close(struct hc11_sci *sci)
  {
    void *ret;
    sigval_t val;

    log_msg(SYS_SCI, 0, "hc11_sci: terminating...\n");
    close(sci->client);
    close(sci->sock);

    val.sival_ptr = sci;
    pthread_sigqueue(sci->thread, SIGUSR1, val);
    pthread_join(sci->thread, &ret);
    log_msg(SYS_SCI, 0, "hc11_sci: thread terminated\n");
    free(sci);
    return 0;
  }

