/* working hc11 simulator for sys11 and others */

#include <stdint.h>
#include <stdio.h>
#include "core.h"
#include "gdbremote.h"

int main(int argc, char **argv)
  {
    struct hc11_core core;
    struct gdbremote_t remote;

    printf("sys11 simulator v0.1 by f4grx (c) 2019\n");

    hc11_core_init(&core);
    hc11_sci_init(&core);

    gdbremote_init(&remote);

    hc11_core_reset(&core);
    while(1)
      {
        hc11_core_clock(&core);
      }

    gdbremote_close(&remote);
  }

