/* gdb remote target for simulator */

#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>

#include "gdbremote.h"

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

    server.sin_addr.s_addr = INADDR_LOOPBACK;
    server.sin_port = ntohs(gr->port);
    ret = bind(gr->sock, (struct sockaddr*)&server, sizeof(server));

    ret = listen(gr->sock, 0);
    gr->running = true;
    //TODO install signal handler for USR1
    while(gr->running)
      {
      int cli;
      int clientsize = sizeof(client);
      cli = accept(gr->sock, (struct sockaddr*)&client, &clientsize);
      FILE *io = fdopen(cli, "rw");
      fprintf(io, "hello kthxbye\r\n");
      fclose(io);
      }

    close(gr->sock);
  }

int gdbremote_init(struct gdbremote_t *gr)
  {
    pthread_t id;
    pthread_create(&id, NULL, gdbremote_thread, gr);
    gr->tid = id;
  }

int gdbremote_close(struct gdbremote_t *gr)
  {
    void *ret;
    pthread_kill(gr->tid, SIGUSR1);
    pthread_join(gr->tid, &ret);
  }

