/* gdb remote target for simulator */

#define _GNU_SOURCE

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

static int gdbremote_tx(struct gdbremote_t *gr, bool req_ack)
  {
    uint8_t csum;
    char ack, tx, rx;
    int ret;
    char sum[3];
    int index;
    int len;
    printf("<<<");

    len = gr->txlen;
    index = 0;
    csum = 0;
    gdbremote_putc('$', gr->client);
    while(len > 0)
      {
        tx = gr->txbuf[index];
        if(tx == '#' || tx == '%' || tx == '}' || tx == '*')
          {
            csum += '}';
            gdbremote_putc('}', gr->client);
            tx ^= 0x20;
          }
        csum = csum + (uint8_t)tx;
        gdbremote_putc(tx, gr->client);
        index++;
        len--;
      }
    gdbremote_putc('#', gr->client);
    sprintf(sum, "%02x", csum&0xFF);
    gdbremote_putc(sum[0], gr->client);
    gdbremote_putc(sum[1], gr->client);
    printf("\n");

    if(req_ack)
      {
        ret = recv(gr->client, &rx, 1, 0);
        if(ret < 0) return -1;
        if(ret == 0) return -1;
        if(rx != '+')
          {
            printf("no ack received (got %02X, '%c')\n", rx, rx);
          }
      }
    return 0;
  }

int gdbremote_txraw(struct gdbremote_t *gr)
  {
    return gdbremote_tx(gr, true);
  }

int gdbremote_txstr(struct gdbremote_t *gr, const char *fmt, ...)
  {
    va_list ap;
    va_start(ap, fmt);
    gr->txlen = vsprintf(gr->txbuf, fmt, ap);
    va_end(ap);
    return gdbremote_tx(gr, true);
  }

int gdbremote_txnotif(struct gdbremote_t *gr, const char *fmt, ...)
  {
    va_list ap;
    va_start(ap, fmt);
    gr->txlen = vsprintf(gr->txbuf, fmt, ap);
    va_end(ap);
    return gdbremote_tx(gr, false);
  }

void gdbremote_monitor(struct gdbremote_t *gr)
  {
    if(!strncmp("help", gr->rxbuf, strlen("help")))
      {
        gr->txlen = sprintf(gr->txbuf, "reset - restart cpu\n");
      }
    else if(!strncmp("reset", gr->rxbuf, strlen("reset")))
      {
        hc11_core_reset(gr->core);
        hc11_core_prep (gr->core);
        gr->txlen = sprintf(gr->txbuf, "target was reset\n");
      }
  }

void gdbremote_query(struct gdbremote_t *gr)
  {
    if(!strncmp("qSupported", gr->rxbuf, strlen("qSupported")))
      {
        //gdb request supported features at boot
        gdbremote_txstr(gr, "PacketSize=%d", GDBREMOTE_MAX_RX);
        //gdbremote_tx(gr, io, "");
      }
    else if(!strncmp("qfThreadInfo", gr->rxbuf, strlen("qfThreadInfo")))
      {
        gdbremote_txstr(gr, "m0");
      }
    else if(!strncmp("qsThreadInfo", gr->rxbuf, strlen("qsThreadInfo")))
      {
        gdbremote_txstr(gr, "l"); //this is a lower case L
      }
    else if(!strncmp("qAttached", gr->rxbuf, strlen("qAttached")))
      {
        gdbremote_txstr(gr, "1"); //this is a one
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
        gdbremote_monitor(gr);
        if(gr->txlen == 0)
          {
            gdbremote_txstr(gr, "");
          }
        else
          {
            char buf[3];
            //convert response to hex
            if(gr->txlen > (GDBREMOTE_MAX_TX/2) - 4)
              {
                gr->txlen = GDBREMOTE_MAX_TX/2 - 4;
              }
            for(i=gr->txlen-1; i>=0; i--)
              {
                sprintf(buf, "%02X", gr->txbuf[i]);
                gr->txbuf[2*i  ] = buf[0];
                gr->txbuf[2*i+1] = buf[1];
              }
            gr->txlen *= 2;
            gdbremote_txraw(gr);
          }
      }
    else if(!strncmp("qC", gr->rxbuf, strlen("qC")))
      {
        gdbremote_txstr(gr, "0");
      }
    else
      {
        printf("Unsupported GDB query\n");
        gdbremote_txstr(gr, "");
      }

  }

void gdbremote_command(struct gdbremote_t *gr)
  {
    printf(">>> %s\n", gr->rxbuf);
    gr->lastcommand = gr->rxbuf[0];

    if(gr->rxbuf[0] == 0x03)
      {
        printf("break request\n");
        gr->core->status = STATUS_STOPPED;
        //no response!
      }
    else if(gr->rxbuf[0] == '?')
      {
        if(gr->core->status == STATUS_STOPPED)
          {
            gdbremote_txstr(gr, "S05"); //core is stopped
          }
        else
          {
            gdbremote_txstr(gr, "S05"); //core is running
          }
      }
    else if(gr->rxbuf[0] == 'c')
      {
        //continue
        gr->core->status = STATUS_RUNNING;
        //no response!
      }
    else if(gr->rxbuf[0] == 'D')
      {
        //detach
        gdbremote_txstr(gr, "OK");
        gr->connected= false;
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
                            (int)gr->core->regs.d >> 8,  //a is MSB
                            (int)gr->core->regs.d & 0xFF,//b is LSB
                            (int)gr->core->regs.ccr
                            );
        gdbremote_txraw(gr);
      }
    else if(gr->rxbuf[0] == 'H')
      {
        //set thread for next operations
        gdbremote_txstr(gr, "OK");
      }
    else if(gr->rxbuf[0] == 'm')
      {
        unsigned int adr, len, i, buf;
        //memory read: MAAAA,len  reply :HH..HH
        if(sscanf(gr->rxbuf+1, "%X,%X", &adr, &len) != 2)
          {
            gdbremote_txstr(gr, "E01");
            return;
          }
        if(len > (GDBREMOTE_MAX_TX/2) - 4)
          {
            len = GDBREMOTE_MAX_TX/2 - 4;
          }
        printf("adr=%04X len=%d\n",adr,len);
        for(i=0;i<len;i++)
          {
            int ch = hc11_core_readb(gr->core, adr+i);
            sprintf(gr->txbuf+(2*i), "%02x", ch);
          }
        gr->txlen = len * 2;
        gdbremote_txraw(gr);
      }
    else if(gr->rxbuf[0] == 'M')
      {
        unsigned int adr, len, count, i, buf;
        char *next;
        //memory write: MAAAA,len:HH..HH
        if(sscanf(gr->rxbuf+1, "%X,%X:%n", &adr, &len, &count) != 2)
          {
            gdbremote_txstr(gr, "E01");
            return;
          }
        next = gr->rxbuf+1+count;
        printf("adr=%04X len=%d\n",adr,len);
        for(i=0;i<len;i++)
          {
            sscanf(next+(2*i), "%02x", &buf);
            hc11_core_writeb(gr->core, adr+i, buf & 0xFF);
          }
        gdbremote_txstr(gr, "OK");
      }
    else if(gr->rxbuf[0] == 'P')
      {
        int reg,val;
        // register write: reg,val
        if(sscanf(gr->rxbuf+1, "%x=%x", &reg, &val) != 2)
          {
            gdbremote_txstr(gr, "E01");
            return;
          }
        printf("set reg %d val %04X\n", reg, val);
        switch(reg)
          {
            case 0: gr->core->regs.x  = val; break;
            case 1: gr->core->regs.d  = val; break;
            case 2: gr->core->regs.y  = val; break;
            case 3: gr->core->regs.sp = val; break;
            case 4: gr->core->regs.pc = val; break;
            case 5: gr->core->regs.d = (gr->core->regs.d & 0x00FF) | (val<<8  ); break; //set a
            case 6: gr->core->regs.d = (gr->core->regs.d & 0xFF00) | (val&0xFF); break; //set b
            case 7: gr->core->regs.ccr = val; break;
            default: gdbremote_txstr(gr, "E02"); return;
          }
        gdbremote_txstr(gr, "OK");
      }
    else if(gr->rxbuf[0] == 'p')
      {
        int reg,val, len=1;
        // register read: reg, return val
        if(sscanf(gr->rxbuf+1, "%x", &reg) != 1)
          {
            gdbremote_txstr(gr, "E01");
            return;
          }
        switch(reg)
          {
            case 0: val = gr->core->regs.x  ; len=2; break;
            case 1: val = gr->core->regs.d  ; len=2; break;
            case 2: val = gr->core->regs.y  ; len=2; break;
            case 3: val = gr->core->regs.sp ; len=2; break;
            case 4: val = gr->core->regs.pc ; len=2; break;
            case 5: val = gr->core->regs.d >> 8;     break; //a is MSB
            case 6: val = gr->core->regs.d & 0xFF;   break; //b is LSB
            case 7: val = gr->core->regs.ccr;        break;
            case 8: val = 0; break; //non existent reg but gdb asks for it
            default: gdbremote_txstr(gr, "E02"); return;
          }
        if(len==1)
          {
            gdbremote_txstr(gr, "%02X", val);
          }
        else
          {
            gdbremote_txstr(gr, "%04X", val);
          }
      }
    else if(gr->rxbuf[0] == 'q')
      {
        gdbremote_query(gr);
      }
    else if(gr->rxbuf[0] == 's')
      {
        //single step
        gr->core->status = STATUS_STEPPING;
        //no response
      }
    else if(gr->rxbuf[0] == 'X')
      {
#if 0
        unsigned int adr, len, count, i, buf;
        uint8_t *next;
        // memory write: XAAAA,len:binary
        if(sscanf(gr->rxbuf+1, "%X,%X:%n", &adr, &len, &count) != 2)
          {
            gdbremote_txstr(gr, "E01");
            return;
          }
        next = (uint8_t*)(gr->rxbuf+1+count);
        printf("adr=%04X len=%d next->%s\n",adr,len,next);
        for(i=0;i<len;i++)
          {
          printf("%02X\n", next[i]);
          }
        gdbremote_txstr(gr, "OK");
#else
        gdbremote_txstr(gr, ""); //not supported
#endif
      }
    else if(gr->rxbuf[0] == 'Z')
      {
        unsigned int type, adr, kind;
        // add breakpoint
        if(sscanf(gr->rxbuf+1, "%d,%X,%d", &type, &adr, &kind) != 3)
          {
            gdbremote_txstr(gr, "E01");
            return;
          }
        if(type == 0 || type == 1)
          {
            printf("set bkpt type %d at %04X\n", type, adr);
            hc11_core_set_bkpt(gr->core, adr);
            gdbremote_txstr(gr, "OK");
          }
        else
          {
            gdbremote_txstr(gr, "E02");
          }
      }
    else if(gr->rxbuf[0] == 'z')
      {
        unsigned int type, adr, kind;
        // remove breakpoint
        if(sscanf(gr->rxbuf+1, "%d,%X,%d", &type, &adr, &kind) != 3)
          {
            gdbremote_txstr(gr, "E01");
            return;
          }
        if(type == 0 || type == 1)
          {
            printf("clr bkpt type %d at %04X\n", type, adr);
            hc11_core_clr_bkpt(gr->core, adr);
            gdbremote_txstr(gr, "OK");
          }
        else
          {
            gdbremote_txstr(gr, "E02");
          }
      }
    else
      {
        gdbremote_txstr(gr, "");
      }
  }

int gdbremote_rx(struct gdbremote_t *gr)
  {
    int state;
    char c,ack;
    char cs[5];
    uint8_t sum;
    int ret;

    state = STATE_WAIT_START;
    while(gr->connected)
      {
        ret = recv(gr->client, &c, 1, 0);
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
                  gdbremote_command(gr);
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
              send(gr->client, &ack, 1, 0);
              if(ack == '+')
                {
                  gr->rxbuf[gr->rxlen] = 0;
                  gdbremote_command(gr);
                }
              state = STATE_WAIT_START;
              break;
          }
      }
  }


static void gdbremote_thread_sig(int num, siginfo_t *info, void *v)
  {
    struct gdbremote_t *sgr = info->si_value.sival_ptr;
    printf("thread signal caught\n");
    sgr->connected = false;
    sgr->running   = false;
  }

static void* gdbremote_thread(void *param)
  {
    struct gdbremote_t *gr = param;
    struct sockaddr_in client;
    struct sigaction sa_thread;
    int ret;

    memset(&sa_thread, 0, sizeof(struct sigaction));
    sa_thread.sa_sigaction = gdbremote_thread_sig;
    sa_thread.sa_flags     = SA_SIGINFO;
    sigaction(SIGUSR1, &sa_thread, NULL);

    gr->running = true;
    printf("gdbremote: listen thread start (port %u)\n", gr->port);
    sem_post(&gr->startstop);

    while(gr->running)
      {
        int clientsize = sizeof(client);
        gr->client = accept(gr->sock, (struct sockaddr*)&client, &clientsize);
        if(gr->client < 0)
          {
            perror("accept()");
            break;
          }
        printf("gdbremote: client connected\n");
        gr->connected = true;
        while(gr->connected)
          {
            ret = gdbremote_rx(gr);
            if(ret < 0) break;
          }
        printf("gdbremote: connection closed\n");
        close(gr->client);
      }

    printf("gdbremote: listen thread done\n");
    return NULL;
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
    sigval_t val;

    printf("gdbremote: terminating...\n");
    close(gr->sock);

    val.sival_ptr = gr;
    pthread_sigqueue(gr->tid, SIGUSR1, val);
    gr->running = false;
    pthread_join(gr->tid, &ret);
    printf("gdbremote: thread terminated\n");
    return 0;
  }

int gdbremote_stopped(struct gdbremote_t *gr)
  {
    printf("core has stopped\n");
    if(gr->lastcommand != 'c' && gr->lastcommand != 's' && gr->lastcommand != 0x03)
      {
        return 0;
      }
    gdbremote_txnotif(gr,"S05"); //this is a notification, there is no ack!
    return 0;
  }

