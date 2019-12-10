/* gdb remote target for simulator */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#include "gdbremote.h"

#define STATE_WAIT_START 1
#define STATE_WAIT_CSUM  2
#define STATE_ESCAPE     3
#define STATE_CSUM_1     4
#define STATE_CSUM_2     5


int gdbremote_putc(const char ch, int client)
  {
  int ret = send(client, &ch, 1, 0);
  fputc(ch, stdout);
  return ret;
  }

int gdbremote_tx(struct gdbremote_t *gr, int client, const char *response)
  {
    uint8_t csum = 0;
    char ack, tx, rx;
    int ret;
    char sum[3];

   printf("<<<");
again:
    gdbremote_putc('$', client);
    while(*response != 0)
      {
        tx = *response;
        if(tx == '#' || tx == '%' || tx == '}' || tx == '*')
          {
            csum += '}';
            gdbremote_putc('}', client);
            tx ^= 0x20;
          }
        csum = csum + (uint8_t)tx;
        gdbremote_putc(tx, client);
        response++;
      }
    gdbremote_putc('#', client);
    sprintf(sum, "%02x", csum&0xFF);
    gdbremote_putc(sum[0], client);
    gdbremote_putc(sum[1], client);
    printf("\n");

    ret = recv(client, &rx, 1, 0);
    if(ret < 0) return -1;
    if(ret == 0) return -1;
    if(rx != '+') goto again;

    return 0;
  }

void gdbremote_command(struct gdbremote_t *gr, int client)
  {
    printf(">>> %s\n", gr->rxbuf);

    if(!strncmp("qSupported", gr->rxbuf, sizeof("qSupported")))
      {
        //gdb request supported features at boot
        gdbremote_tx(gr, client, "PacketSize=1024");
        //gdbremote_tx(gr, io, "");
      }
    else if(!strncmp("qC", gr->rxbuf, sizeof("qC")))
      {
        gdbremote_tx(gr, client, "0");
      }
    else if(!strncmp("qfThreadInfo", gr->rxbuf, sizeof("qfThreadInfo")))
      {
        gdbremote_tx(gr, client, "m0");
      }
    else if(!strncmp("qsThreadInfo", gr->rxbuf, sizeof("qsThreadInfo")))
      {
        gdbremote_tx(gr, client, "l"); //this is a lower case L
      }
    else if(!strncmp("qAttached", gr->rxbuf, sizeof("qAttached")))
      {
        gdbremote_tx(gr, client, "1"); //this is a one
      }
    else if(gr->rxbuf[0] == '?')
      {
        gdbremote_tx(gr, client, "S00");
      }
    else if(gr->rxbuf[0] == 'g')
      {
/* according to gdb/m68hc11-tdep.c:
#define HARD_X_REGNUM   0
#define HARD_D_REGNUM   1
#define HARD_Y_REGNUM   2
#define HARD_SP_REGNUM  3
#define HARD_PC_REGNUM  4

#define HARD_A_REGNUM   5
#define HARD_B_REGNUM   6
#define HARD_CCR_REGNUM 7
 */
        char regs[27];
        sprintf(regs, "%04X%04X%04X%04X%04X%02X%02X%02X",
                gr->core->regs.x,
                gr->core->regs.d,
                gr->core->regs.y,
                gr->core->regs.sp,
                gr->core->regs.pc,
                gr->core->regs.d & 0xFF,
                gr->core->regs.d >> 8,
                gr->core->regs.ccr
               );
        gdbremote_tx(gr, client, regs);
      }
    else if(gr->rxbuf[0] == 'H')
      {
        gdbremote_tx(gr, client, "OK");
      }
    else
      {
end:
        gdbremote_tx(gr, client, "");
      }
  }

int gdbremote_rx(struct gdbremote_t *gr, int client)
  {
    int state;
    char c,ack;
    char cs[5];
    uint8_t sum;
    int ret;

    state = STATE_WAIT_START;
    while(1)
      {
        ret = recv(client, &c, 1, 0);
        if(ret < 0) return -1;
        if(ret == 0) return -1;

        if(c == EOF)
          {
            break;
          }
      //  printf(">> %02X (%c)\n", c, c);

        switch(state)
          {
            case STATE_WAIT_START:
              if(c == 0x03)
                {
                  //special case
                  gr->rxbuf[0] = c;
                  gr->rxlen = 1;
                  gdbremote_command(gr, client);
                }
              else if(c == '$')
                {
                  gr->rxlen = 0;
                  sum       = 0;
                  state     = STATE_WAIT_CSUM;
                }
              break;

            case STATE_WAIT_CSUM:
              if(c == '}')
                {
                  sum = sum + (uint8_t)c;
                  state = STATE_ESCAPE;
                }
              else if(c == '#')
                {
                  state = STATE_CSUM_1;
                }
              else if(gr->rxlen < MAX_RX)
                {
                  gr->rxbuf[gr->rxlen] = c;
                  sum = sum + (uint8_t)c;
                  gr->rxlen += 1;
                }
              else
                {
                  printf("gdbremote: rx buf ovf prevented\n");
                }
              break;

            case STATE_ESCAPE:
              sum = sum + (uint8_t)c;
              gr->rxbuf[gr->rxlen] = c ^ 0x20;
              gr->rxlen += 1;
              state = STATE_WAIT_CSUM;
              break;

            case STATE_CSUM_1:
              cs[0] = c;
              state = STATE_CSUM_2;
              break;

            case STATE_CSUM_2:
              cs[1] = c;
              sprintf(cs+2, "%02x", sum & 0xFF);
              if(cs[0] == cs[2] && cs[1] == cs[3])
                {
                  ack = '+';
                }
              else
                {
                  ack = '-';
                }
              send(client, &ack, 1, 0);
              if(ack == '+')
                {
                  gr->rxbuf[gr->rxlen] = 0;
                  gdbremote_command(gr, client);
                }
              state = STATE_WAIT_START;
              break;
          }
      }
  }

static void* gdbremote_thread(void *param)
  {
    struct gdbremote_t *gr = param;
    struct sockaddr_in client;
    int ret;

    gr->running = true;
    printf("gdbremote: listen thread start (port %u)\n", gr->port);
    sem_post(&gr->startstop);

    while(gr->running)
      {
        int cli;
        int clientsize = sizeof(client);
        cli = accept(gr->sock, (struct sockaddr*)&client, &clientsize);
        if(cli < 0)
          {
            perror("accept()");
            break;
          }
        printf("gdbremote: client connected\n");
        while(1)
          {
            ret = gdbremote_rx(gr, cli);
            if(ret < 0) break;
          }
        printf("gdbremote: connection closed\n");
        close(cli);
      }

    printf("gdbremote: listen thread done\n");
  }

int gdbremote_init(struct gdbremote_t *gr)
  {
    pthread_t id;
    struct sockaddr_in server;
    int ret;
    int yes = 1;

    printf("gdbremote starting\n");
    sem_init(&gr->startstop, 0, 0);

    // create tcp socket to allow gdb incoming connection
    gr->sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(gr->sock < 0)
      {
        return -1;
      }

    ret = setsockopt(gr->sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    if(ret < 0)
      {
        perror("setsockopt SO_REUSEADDR");
        return -1;
      }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server.sin_port = ntohs(gr->port);
    ret = bind(gr->sock, (struct sockaddr*)&server, sizeof(server));
    if(ret < 0)
      {
        perror("bind()");
        goto close;
      }

    ret = listen(gr->sock, 0);
    if(ret < 0)
      {
        perror("listen()");
        goto close;
      }


    pthread_create(&id, NULL, gdbremote_thread, gr);

    sem_wait(&gr->startstop);
    printf("gdbremote started\n");
    gr->tid = id;
    return 0;

close:
    close(gr->sock);
    return -1;
  }

int gdbremote_close(struct gdbremote_t *gr)
  {
    void *ret;
    printf("gdbremote: terminating...\n");
    close(gr->sock);
pthread_kill(gr->tid, SIGINT);
    gr->running = false;
    pthread_join(gr->tid, &ret);
    printf("gdbremote: thread terminated\n");
    return 0;
  }

