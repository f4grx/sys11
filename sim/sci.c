#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#include "core.h"

enum
  {
    OFF_BAUD,
    OFF_SCCR1,
    OFF_SCCR2,
    OFF_SCSR,
    OFF_SCDR,
    REGCNT
  };

struct hc11_sci
  {
    uint8_t regs[REGCNT];
  };

static uint8_t sci_read(void *ctx, uint16_t off)
  {
    struct hc11_sci *sci = ctx;
  }

static void sci_write(void *ctx, uint16_t off, uint8_t val)
  {
    struct hc11_sci *sci = ctx;
  }

void hc11_sci_init(struct hc11_core *core)
  {
    struct hc11_sci *sci;

    sci = malloc(sizeof(struct hc11_sci));

    hc11_core_iocallback(core, 0x2B, 5, sci, sci_read, sci_write);
  }

