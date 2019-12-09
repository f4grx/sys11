/* gdb remote target for simulator */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#include "gdbremote.h"

#define STATE_WAIT_START 1
#define STATE_WAIT_CSUM  2
#define STATE_CSUM_1     3
#define STATE_CSUM_2     4

void gdbremote_tx(struct gdbremote_t *gr, FILE *io, const char *response)
  {
    char csum = 0;
    fputc('$', io);
    while(*response != 0)
      {
        csum = csum + *response;
        fputc(*response, io);
        response++;
      }
    fputc('#', io); fprintf(io, "%02X", csum); fflush(io);
  }

void gdbremote_command(struct gdbremote_t *gr, FILE *io)
  {
    char *params;

    printf(">>> %s\n", gr->rxbuf);

    params = strstr(gr->rxbuf, ":");
    if(params == NULL)
      {
        goto end;
      }
    *params = 0;
    params++;

    printf("cmd: %s\n", gr->rxbuf);

   if(!strcmp(gr->rxbuf, "qSupported"))
    {
      //gdb request supported features at boot
      //gdbremote_tx(gr, io, "qSupported:PacketSize=1024");
      goto end; 
    }
  else
    {
end:
      fprintf(io, "+$#00"); fflush(io);
    }
  }

void gdbremote_client(struct gdbremote_t *gr, FILE *io)
  {
    int index;
    int state;
    char c;
    char cs[2];

    printf("gdbremote: client connected\n");

    state = STATE_WAIT_START;
    while(1)
      {
        c = fgetc(io);
        if(c == EOF)
          {
            printf("gdbremote: connection closed\n");
            break;
          }
        //printf(">> %02X (%c)\n", c, c);
        switch(state)
          {
            case STATE_WAIT_START:
              if(c == '$')
                {
                  index = 0;
                  state = STATE_WAIT_CSUM;
                }
              break;

            case STATE_WAIT_CSUM:
              if(c == '#')
                {
                  gr->rxbuf[index] = 0;
                  state = STATE_CSUM_1;
                }
              else if(index < (sizeof(gr->rxbuf)-1))
                {
                  gr->rxbuf[index++] = c;
                }
              else
                {
                  printf("gdbremote: rx buf ovf prevented\n");
                }
              break;

            case STATE_CSUM_1:
              cs[0] = c;
              state = STATE_CSUM_2;
              break;

            case STATE_CSUM_2:
              cs[1] = c;
              state = STATE_WAIT_START;
              fprintf(io, "+"); fflush(io);
              gdbremote_command(gr, io);
              break;
          }
      }
  }

static void* gdbremote_thread(void *param)
  {
    struct gdbremote_t *gr = param;
    struct sockaddr_in server;
    struct sockaddr_in client;
    int ret;

    // create tcp socket to allow gdb incoming connection
    gr->sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(gr->sock < 0)
      {
        return (void*)-1;
      }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server.sin_port = ntohs(gr->port);
    ret = bind(gr->sock, (struct sockaddr*)&server, sizeof(server));
    if(ret < 0)
      {
        perror("bind()");
      }

    ret = listen(gr->sock, 0);
    if(ret < 0)
      {
        perror("listen()");
      }

    gr->running = true;
    printf("gdbremote: listen thread start (port %u)\n", gr->port);
    sem_post(&gr->startstop);

    //TODO install signal handler for USR1
    while(gr->running)
      {
      int cli;
      int clientsize = sizeof(client);
      cli = accept(gr->sock, (struct sockaddr*)&client, &clientsize);
      FILE *io = fdopen(cli, "rw");
      gdbremote_client(gr, io);
      fclose(io);
      }

    close(gr->sock);
    printf("gdbremote: listen thread done\n");
  }

int gdbremote_init(struct gdbremote_t *gr)
  {
    pthread_t id;
    printf("gdbremote starting\n");
    sem_init(&gr->startstop, 0, 0);
    pthread_create(&id, NULL, gdbremote_thread, gr);
    sem_wait(&gr->startstop);
    printf("gdbremote started\n");
    gr->tid = id;
  }

int gdbremote_close(struct gdbremote_t *gr)
  {
    void *ret;
//    pthread_kill(gr->tid, SIGUSR1);
    gr->running = false;
    pthread_join(gr->tid, &ret);
  }

