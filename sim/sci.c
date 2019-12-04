#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "core.h"

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

struct hc11_sci
  {
    uint8_t regs[REGCNT];
  };

static uint8_t sci_read(void *ctx, uint16_t off)
  {
    struct hc11_sci *sci = ctx;
    uint8_t ret;
    off -= SCI_REG_FIRST;
    ret = sci->regs[off];
    switch(off)
      {
      case OFF_BAUD:  printf("SCI read BAUD: %02X\n", sci->regs[off]);  break;
      case OFF_SCCR1: printf("SCI read SCCR1: %02X\n", sci->regs[off]); break;
      case OFF_SCCR2: printf("SCI read SCCR2: %02X\n", sci->regs[off]); break;
      case OFF_SCSR:  printf("SCI read SCSR: %02X\n", sci->regs[off]);  break;
      case OFF_SCDR:  printf("SCI read SCDR: %02X\n", sci->regs[off]);  break;
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
      case OFF_BAUD:  printf("SCI BAUD <- %02X\n", val); break;
      case OFF_SCCR1: printf("SCI SCCR1 <- %02X\n", val); break;
      case OFF_SCCR2: printf("SCI SCCR2 <- %02X\n", val); break;
      case OFF_SCSR:  printf("SCI SCSR <- %02X\n", val); break;
      case OFF_SCDR:  printf("SCI SCDR <- %02X\n", val); break;
      }
  }

void hc11_sci_init(struct hc11_core *core)
  {
    struct hc11_sci *sci;

    sci = malloc(sizeof(struct hc11_sci));
    memset(sci->regs, 0, 5);
    sci->regs[OFF_SCSR] = 0x80; //transmit is always complete
    hc11_core_iocallback(core, SCI_REG_FIRST, 5, sci, sci_read, sci_write);
  }

