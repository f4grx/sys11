#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "core.h"

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

