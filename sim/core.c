#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "core.h"

// Define addressing modes
enum
  {
    ILL, //illegal opcode
    INH, //inherent (no operand, direct execution
    IM1, //immediate, one byte
    IM2, //immediate, two bytes
    DIR, //direct (one byte abs address)
    REL, //relative (branches)
    EXT, //extended (two bytes absolute address)
    INX, //indexed relative to X
    INY, //indexed relative to Y
    TDI, //direct, triple for brset/clr dd/mm/rr
    TIX, //indirect X, triple for brset/clr ff/mm/rr
    TIY, //indirect Y, triple for brset/clr ff/mm/rr
    DDI, //direct, double for bset/clr dd/mm/rr
    DIX, //indirect X, double for bset/clr ff/mm/rr
    DIY, //indirect Y, double for bset/clr ff/mm/rr
  };
//last 18ad
static const uint8_t opmodes = 
  {
/*00  01  02  03  04  05  06  07  08  09  0A  0B  0C  0D  0E  0F*/
  INH,INH,INH,INH,INH,INH,INH,INH,INH,INH,INH,0,  INH,INH,INH,INH, /* 00-0F */
  INH,INH,TDI,TDI,DDI,DDI,INH,INH,0,  INH,0,  INH,DIX,DIX,TIX,TIX, /* 10-1F */
  REL,REL,REL,REL,REL,REL,REL,REL,REL,REL,REL,REL,REL,REL,REL,REL, /* 20-2F */
  INH,INH,INH,INH,INH,INH,INH,INH,INH,INH,INH,INH,INH,INH,INH,INH, /* 30-3F */
  INH,0,  0,  INH,INH,0,  INH,INH,INH,INH,INH,0,  INH INH,0,  INH, /* 40-4F */
  INH,0,  0,  INH,INH,0,  INH,INH,INH,INH,INH,0,  INH,INH,0,  INH, /* 50-5F */
  INX,0,  0,  INX,INX,0,  INX,INX,INX,INX,INX,0,  INX,INX,INX,INX, /* 60-5F */
  EXT,0,  0,  EXT,EXT,0,  EXT,EXT,EXT,EXT,EXT,0,  EXT,EXT,EXT,EXT, /* 70-7F */
  IM1,IM1,IM1,IM2,IM1,IM1,IM1,0,  IM1,IM1,IM1,IM1,IM2,REL,IM2,INH, /* 80-8F */
  DIR,DIR,DIR,DIR,DIR,DIR,DIR,DIR,DIR,DIR,DIR,DIR,DIR,DIR,DIR,DIR, /* 90-9F */
  INX,INX,INX,INX,INX,INX,INX,INX,INX,INX,INX,INX,INX,INX,INX,INX, /* A0-AF */
  EXT,EXT,EXT,EXT,EXT,EXT,EXT,EXT,EXT,EXT,EXT,EXT,EXT,EXT,EXT,EXT, /* B0-BF */
  IM1,IM1,IM1,IM2,IM1,IM1,IM1,0,  IM1,IM1,IM1,IM1,IM2,0,  IM2,INH, /* C0-CF */
  DIR,DIR,DIR,DIR,DIR,DIR,DIR,DIR,DIR,DIR,DIR,DIR,DIR,DIR,DIR,DIR, /* D0-DF */
  INX,INX,INX,INX,INX,INX,INX,INX,INX,INX,INX,INX,INX,INX,INX,INX, /* E0-EF */
  EXT,EXT,EXT,EXT,EXT,EXT,EXT,EXT,EXT,EXT,EXT,EXT,EXT,EXT,EXT,EXT, /* F0-FF */
  };

static const uint8_t opmodes_18 = 
  {
/*00  01  02  03  04  05  06  07  08  09  0A  0B  0C  0D  0E  0F*/
  0,  0,  0,  0,  0,  0,  0,  0,  INH,INH,0,  0,  0,  0,  0,  0, /* 00-0F */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  DIY,DIY,TIY,TIY, /* 10-1F */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 20-2F */
  INH,0,  0,  0,  0,  INH,0,  0,  INH,0,  INH,0,  INH,0,  0,  0, /* 30-3F */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 40-4F */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 50-5F */
  INY,0,  0,  INY,INY,0,  INY,INY,INY,INY,INY,0,  INY,INY,INY,INY, /* 60-5F */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 70-7F */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  IM2,0,  0,  INH, /* 80-8F */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  DIR,0,  0,  0, /* 90-9F */
  INY,INY,INY,INY,INY,INY,INY,INY,INY,INY,INY,INY,INY,INY,INY,INY, /* A0-AF */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  EXT,0,  0,  0, /* B0-BF */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  IM2,0, /* C0-CF */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  DIR,DIR, /* D0-DF */
  INY,INY,INY,INY,INY,INY,INY,INY,INY,INY,INY,INY,INY,INY,INY,INY, /* E0-EF */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  EXT,EXT, /* F0-FF */
  };

static const uint8_t opmodes_1A = 
  {
/*00  01  02  03  04  05  06  07  08  09  0A  0B  0C  0D  0E  0F*/
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 00-0F */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 10-1F */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 20-2F */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 30-3F */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 40-4F */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 50-5F */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 60-5F */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 70-7F */
  0,  0,  0,  IM2,0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 80-8F */
  0,  0,  0,  DIR,0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 90-9F */
  0,  0,  0,  INX,0,  0,  0,  0,  0,  0,  0,  0,  INX,0,  0,  0, /* A0-AF */
  0,  0,  0,  EXT,0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* B0-BF */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* C0-CF */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* D0-DF */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  INX,INX, /* E0-EF */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* F0-FF */
  };

static const uint8_t opmodes_CD = 
  {
/*00  01  02  03  04  05  06  07  08  09  0A  0B  0C  0D  0E  0F*/
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 00-0F */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 10-1F */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 20-2F */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 30-3F */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 40-4F */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 50-5F */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 60-5F */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 70-7F */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 80-8F */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 90-9F */
  0,  0,  0,  INY,0,  0,  0,  0,  0,  0,  0,  0,  INY,0,  0,  0, /* A0-AF */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* B0-BF */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* C0-CF */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* D0-DF */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  INY,INY, /* E0-EF */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* F0-FF */
  };

static uint8_t ram_read(void *ctx, uint16_t off)
  {
    uint8_t *mem = ctx;
    return mem[off];
  }

static void ram_write(void* ctx, uint16_t off, uint8_t val)
  {
    uint8_t *mem = ctx;
    mem[off] = val;
  }

static uint8_t hc11_core_readb(struct hc11_core *core, uint16_t adr)
  {
    struct hc11_mapping *cur;
    uint8_t ret;

    //prio: fist IO, then internal mem [ram], then ext mem [maps]
    if(adr >= core->iobase && adr < (core->iobase + 0x40))
      {
        //reading a reg
        struct hc11_io *reg = &core->io[adr - core->iobase];
        if(reg->rdf == NULL)
          {
            printf("READ @ 0x%04X -> 0xFF [reg-none]\n", adr);
            return 0xFF;
          }
        else
          {
            ret = reg->rdf(reg->ctx, adr - core->iobase);
            printf("READ @ 0x%04X -> %02X [reg]\n", adr, ret);
            return ret;
          }
      }

    //not reading a reg. try iram
    if(adr >= core->rambase && adr < (core->rambase + 256))
      {
        ret = core->iram[adr - core->rambase];
        printf("READ @ 0x%04X -> %02X [iram]\n", adr, ret);
        return ret;
      }

    cur = core->maps;
    while(cur != NULL)
      {
        if(adr >= cur->start && adr < (cur->start + cur->len))
          {
            ret = cur->rdf(cur->ctx, adr - cur->start);
            printf("READ @ 0x%04X -> %02X [xmem]\n", adr, ret);
            return ret;
          }
        cur = cur->next;
      }

    //not io, not iram -> find adr in mappings
    printf("READ @ 0x%04X -> 0xFF [none]\n", adr);
    return 0xFF;
  }

static uint8_t hc11_core_writeb(struct hc11_core *core, uint16_t adr,
                                uint8_t val)
  {
    struct hc11_mapping *cur;

    printf("CORE write @ 0x%04X (%02X)\n", adr, val);
    if(adr >= core->iobase && adr < core->iobase + 0x40)
      {
        //reading a reg
        struct hc11_io *reg = &core->io[adr - core->iobase];
        if(reg->wrf == NULL)
          {
            printf("writing readonly/unimplemented reg\n");
          }
        else
          {
            reg->wrf(reg->ctx, adr - core->iobase, val);
          }
      }

    //not reading a reg. try iram
    if(adr >= core->rambase && adr < core->rambase + 256)
      {
        core->iram[adr - core->rambase] = val;
      }

    printf("Write to undefined address ignored\n");
  }

void hc11_core_map(struct hc11_core *core, uint16_t start, uint16_t count,
                   void *ctx, read_f rd, write_f wr)
  {
    struct hc11_mapping *map = malloc(sizeof(struct hc11_mapping));
    struct hc11_mapping *cur, *next;

    map->next  = NULL;
    map->start = start;
    map->len   = count;
    map->ctx   = ctx;
    map->rdf   = rd;
    map->wrf   = wr;

    cur = core->maps;
    if(!cur)
      {
        core->maps = map;
        return;
      }
    else if(start < cur->start)
      {
        map->next = cur;
        core->maps = map;
        return;
      }

    while(cur != NULL)
      {
        if(start > cur->start)
          {
            map->next = cur->next;
            cur->next = map;
            break;
          }
        cur = cur->next;
      }
  }

void hc11_core_map_ram(struct hc11_core *core, uint16_t start,
                          uint16_t count)
  {
    uint8_t *ram;
    ram = malloc(count);
    memset(ram,0,count);
    hc11_core_map(core, start, count, ram, ram_read, ram_write);
  }

void hc11_core_map_rom(struct hc11_core *core, uint16_t start, uint16_t count, uint8_t *rom)
  {
    hc11_core_map(core, start, count, rom, ram_read, NULL);
  }

void hc11_core_iocallback(struct hc11_core *core, uint8_t off, uint8_t count,
                          void *ctx, read_f rd, write_f wr)
  {
    uint8_t i;
    for(i=0;i<count;i++)
      {
        core->io[off].ctx = ctx;
        core->io[off].rdf = rd;
        core->io[off].wrf = wr;
        off++;
      }
  }

void hc11_core_init(struct hc11_core *core)
  {
    int i;
    core->maps = NULL;
    for(i=0;i<64;i++)
      {
        core->io[i].rdf = NULL;
        core->io[i].wrf = NULL;
      }
  }

void hc11_core_reset(struct hc11_core *core)
  {
    core->rambase = 0x0000;
    core->iobase  = 0x1000;
    core->vector  = VECTOR_RESET;
    core->state   = STATE_VECTORFETCH_H;
    core->prefix  = 0x00;
    core->clocks = 0;
  }

void hc11_core_clock(struct hc11_core *core)
  {
    uint8_t tmp;
    core->clocks += 1;
    switch(core->state)
      {
        case STATE_VECTORFETCH_H:
          printf("VECTOR fetch @ 0x%04X\n", core->vector);
          core->regs.pc = hc11_core_readb(core,core->vector) << 8;
          core->state = STATE_VECTORFETCH_L;
          break;

        case STATE_VECTORFETCH_L:
          core->regs.pc |= hc11_core_readb(core,core->vector+1);
          core->state = STATE_FETCHOPCODE;
          break;

        case STATE_FETCHOPCODE:
          tmp = hc11_core_readb(core,core->regs.pc);
          core->regs.pc = core->regs.pc + 1;           
          if(tmp == 0x18 || tmp == 0x1A || tmp == 0xCD)
            {
              if(core->prefix == 0)
                {
                  core->prefix = tmp;
                  break; //stay in this state
                }
              else
                {
                  //prefix already set: illegal
                  core->vector  = VECTOR_ILLEGAL;
                  core->state   = STATE_VECTORFETCH_H;
                  break;
                }
            }
          else
            {
            //not a prefix
            core->opcode = tmp;
            core->state = STATE_EXECUTE; //actual next state depends on adressing mode
            }
          break;          
        case STATE_EXECUTE:
          core->prefix = 0; //last action
          break;

      }//switch
  }

