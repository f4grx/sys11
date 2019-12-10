/* gdb remote target for simulator */

#include <stdbool.h>
#include <stdarg.h>
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

int gdbremote_txraw(struct gdbremote_t *gr, int client)
  {
    uint8_t csum = 0;
    char ack, tx, rx;
    int ret;
    char sum[3];
    int index=0;

   printf("<<<");
again:
    gdbremote_putc('$', client);
    while(gr->txlen > 0)
      {
        tx = gr->txbuf[index];
        if(tx == '#' || tx == '%' || tx == '}' || tx == '*')
          {
            csum += '}';
            gdbremote_putc('}', client);
            tx ^= 0x20;
          }
        csum = csum + (uint8_t)tx;
        gdbremote_putc(tx, client);
        index++;
        gr->txlen--;
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

int gdbremote_txstr(struct gdbremote_t *gr, int client, const char *fmt, ...)
  {
    va_list ap;
    va_start(ap, fmt);
    gr->txlen = vsprintf(gr->txbuf, fmt, ap);
    va_end(ap);
    return gdbremote_txraw(gr,client);
  }

void gdbremote_monitor(struct gdbremote_t *gr, int client)
  {
    if(!strncmp("help", gr->rxbuf, strlen("help")))
      {
        gr->txlen = sprintf(gr->txbuf, "reset - restart cpu\n");
      }
    else if(!strncmp("reset", gr->rxbuf, strlen("reset")))
      {
        hc11_core_reset(gr->core);
        hc11_core_prep (gr->core);
        gr->txlen = sprintf(gr->txbuf, "target reset");
      }
  }

void gdbremote_query(struct gdbremote_t *gr, int client)
  {
    if(!strncmp("qSupported", gr->rxbuf, strlen("qSupported")))
      {
        //gdb request supported features at boot
        gdbremote_txstr(gr, client, "PacketSize=%d", GDBREMOTE_MAX_RX);
        //gdbremote_tx(gr, io, "");
      }
    else if(!strncmp("qfThreadInfo", gr->rxbuf, strlen("qfThreadInfo")))
      {
        gdbremote_txstr(gr, client, "m0");
      }
    else if(!strncmp("qsThreadInfo", gr->rxbuf, strlen("qsThreadInfo")))
      {
        gdbremote_txstr(gr, client, "l"); //this is a lower case L
      }
    else if(!strncmp("qAttached", gr->rxbuf, strlen("qAttached")))
      {
        gdbremote_txstr(gr, client, "1"); //this is a one
      }
    else if(!strncmp("qRcmd,", gr->rxbuf, strlen("qRcmd,")))
      {
        int i,buf;
        //convert hex to chars
        for(i=0;i<(gr->rxlen-6)/2;i++)
          {
            sscanf(gr->rxbuf + 6 + (2 * i), "%02x", &buf);
            gr->rxbuf[i] = buf & 0xFF;
          }
        gr->rxbuf[i] = 0;
        printf("monitor: %s\n", gr->rxbuf);
        gr->txlen = 0;
        gdbremote_monitor(gr, client);
        if(gr->txlen == 0)
          {
            gdbremote_txstr(gr, client, "");
          }
        else
          {
            char buf[3];
            //convert response to hex
            if(gr->txlen > (GDBREMOTE_MAX_TX/2))
              {
                gr->txlen = GDBREMOTE_MAX_TX/2;
              }
            for(i=gr->txlen-1; i>=0; i--)
              {
                sprintf(buf, "%02X", gr->txbuf[i]);
                gr->txbuf[2*i  ] = buf[0];
                gr->txbuf[2*i+1] = buf[1];
              }
            gr->txlen *= 2;
            gdbremote_txraw(gr, client);
          }
      }
    else if(!strncmp("qC", gr->rxbuf, strlen("qC")))
      {
        gdbremote_txstr(gr, client, "0");
      }
    else
      {
        printf("Unsupported GDB query\n");
        gdbremote_txstr(gr, client, "");
      }

  }

void gdbremote_command(struct gdbremote_t *gr, int client)
  {
    printf(">>> %s\n", gr->rxbuf);

    if(gr->rxbuf[0] == 'q')
      {
        gdbremote_query(gr, client);
      }
    else if(gr->rxbuf[0] == '?')
      {
        gdbremote_txstr(gr, client, "S05");
      }
    else if(gr->rxbuf[0] == 'g')
      {
/* According to gdb/m68hc11-tdep.c:
 * #define HARD_X_REGNUM   0
 * #define HARD_D_REGNUM   1
 * #define HARD_Y_REGNUM   2
 * #define HARD_SP_REGNUM  3
 * #define HARD_PC_REGNUM  4
 * #define HARD_A_REGNUM   5
 * #define HARD_B_REGNUM   6
 * #define HARD_CCR_REGNUM 7
 */
        gr->txlen = sprintf(gr->txbuf, "%04X%04X%04X%04X%04X%02X%02X%02X",
                            (int)gr->core->regs.x,
                            (int)gr->core->regs.d,
                            (int)gr->core->regs.y,
                            (int)gr->core->regs.sp,
                            (int)gr->core->regs.pc,
                            (int)gr->core->regs.d & 0xFF,
                            (int)gr->core->regs.d >> 8,
                            (int)gr->core->regs.ccr
                            );
        gdbremote_txraw(gr, client);
      }
    else if(gr->rxbuf[0] == 'H')
      {
        //set thread for next operations
        gdbremote_txstr(gr, client, "OK");
      }
    else if(gr->rxbuf[0] == 's')
      {
        //single step
        hc11_core_insn(gr->core);
        gdbremote_txstr(gr, client, "S05");
      }
    else
      {
        gdbremote_txstr(gr, client, "");
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
              else if(gr->rxlen < GDBREMOTE_MAX_RX)
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

