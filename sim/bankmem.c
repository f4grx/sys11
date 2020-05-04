#include <stdbool.h>
#include "bankmem.h"
#include "core.h"
#include "log.h"

#include <stdlib.h>

struct bankedmem
  {
    uint8_t *storage;
    uint16_t numpages;
    uint16_t pagesize;
    uint16_t curpage;
  };

uint8_t page_read(void *ctx, uint16_t off)
  {
    struct bankedmem *bank = (struct bankedmem*)ctx;
    log_msg(SYS_CORE, CORE_MEM, "read off %04X curpage=%d\n", off, bank->curpage);
    
    if(bank->curpage >= bank->numpages)
      {
        return 0xFF;
      }
    return bank->storage[(bank->curpage * bank->pagesize) + off];
  }

void page_write(void *ctx, uint16_t off, uint8_t val)
  {
    struct bankedmem *bank = (struct bankedmem*)ctx;
    log_msg(SYS_CORE, CORE_MEM, "write off %04X curpage=%d\n", off, bank->curpage);
    if(bank->curpage >= bank->numpages)
      {
        return;
      }
    bank->storage[(bank->curpage * bank->pagesize) + off] = val;
  }

void pagesel_hi_write(void *ctx, uint16_t off, uint8_t val)
  {
    struct bankedmem *bank = (struct bankedmem*)ctx;
    bank->curpage = (bank->curpage & 0x00FF) & ((uint16_t)val << 8);
    log_msg(SYS_CORE, CORE_MEM, "wrhi-> %02X curpage=%d\n", val, bank->curpage);
  }

void pagesel_lo_write(void *ctx, uint16_t off, uint8_t val)
  {
    struct bankedmem *bank = (struct bankedmem*)ctx;
    bank->curpage = (bank->curpage & 0xFF00) & ((uint16_t)val);
    log_msg(SYS_CORE, CORE_MEM, "wrlo-> %02X curpage=%d\n", val, bank->curpage);
  }

int hc11_bankmem_attach(struct hc11_core *core,
                        uint16_t pagebase, uint16_t pagelen, uint16_t numpages,
                        uint16_t pagesel_hi, uint16_t pagesel_lo)
  {
    uint16_t totalmem;
    struct bankedmem *bank;

    bank = malloc(sizeof(struct bankedmem));
    if(!bank)
      {
        return -1;
      }

    totalmem = pagelen * numpages;
    bank->storage = malloc(totalmem);
    if(!bank->storage)
      {
        free(bank);
        return -1;
      }
    bank->numpages = numpages;
    bank->pagesize = pagelen;
    bank->curpage  = 0;

    hc11_core_map(core, "banked", pagebase, pagelen, bank, page_read, page_write);
    if(pagesel_hi != 0)
      {
        hc11_core_map(core, "banksel_hi", pagesel_hi, 1, bank, NULL, pagesel_hi_write);
      }
    hc11_core_map(core, "banksel_lo", pagesel_lo, 1, bank, NULL, pagesel_lo_write);

    log_msg(SYS_CORE, CORE_MEM, "BANKED: npage %u pgsiz %u base %04X selhi %04X sello %04X\n",numpages,pagelen,pagebase,pagesel_hi,pagesel_lo);

    return 0;
  }

