/* gdb remote target for simulator */

#include "gdbremote.h"

int gdbremote_init(struct gdbremote_t *gr)
  {
    // create tcp socket to allow gdb incoming connection
    gr->sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(gr->sock < 0)
      {
        return -1;
      }

  }

int gdbremote_close(struct gdbremote_t *gr)
  {
    close(gr->sock);
  }
