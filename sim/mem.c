#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

uint8_t hc11_core_readb(struct hc11_core *core, uint16_t adr)
  {
    struct hc11_mapping *cur;
    uint8_t ret;

    printf("[%8ld] ", core->clocks); fflush(stdout);
    //prio: fist IO, then internal mem [ram], then ext mem [maps]
    if(adr >= core->iobase && adr < (core->iobase + 0x40))
      {
        //reading a reg
        struct hc11_io *reg = &core->io[adr - core->iobase];
        if(reg->rdf != NULL)
          {
            ret = reg->rdf(reg->ctx, adr - core->iobase);
            printf("READ  @ 0x%04X -> %02X [reg] rdf=%p\n", adr, ret, reg->rdf);
            return ret;
          }
      }

    //not reading a reg. try iram
    if(adr >= core->rambase && adr < (core->rambase + 256))
      {
        ret = core->iram[adr - core->rambase];
        printf("READ  @ 0x%04X -> %02X [iram]\n", adr, ret);
        return ret;
      }

    cur = core->maps;
    while(cur != NULL)
      {
        if(adr >= cur->start && adr < (cur->start + cur->len))
          {
            ret = cur->rdf(cur->ctx, adr - cur->start);
            printf("READ  @ 0x%04X -> %02X [xmem/%s]\n", adr, ret, cur->name);
            return ret;
          }
        cur = cur->next;
      }

    //not io, not iram -> find adr in mappings
    printf("READ  @ 0x%04X -> 0xFF [none]\n", adr);
    return 0xFF;
  }

void hc11_core_writeb(struct hc11_core *core, uint16_t adr,
                                uint8_t val)
  {
    struct hc11_mapping *cur;

    printf("[%8ld] ", core->clocks); fflush(stdout);
    if(adr >= core->iobase && adr < core->iobase + 0x40)
      {
        //reading a reg
        struct hc11_io *reg = &core->io[adr - core->iobase];
        if(reg->wrf != NULL)
          {
            printf("WRITE @ 0x%04X <- %02X [reg] wrf=%p\n", adr, val, reg->wrf);
            reg->wrf(reg->ctx, adr - core->iobase, val);
            return;
          }
      }

    //not reading a reg. try iram
    if(adr >= core->rambase && adr < core->rambase + 256)
      {
        printf("WRITE @ 0x%04X <- %02X [iram]\n", adr, val);
        core->iram[adr - core->rambase] = val;
        return;
      }

    cur = core->maps;
    while(cur != NULL)
      {
        if(adr >= cur->start && adr < (cur->start + cur->len))
          {
            if(cur->wrf)
              {
                printf("WRITE @ 0x%04X <- %02X [xmem/%s]\n", adr, val, cur->name);
                cur->wrf(cur->ctx, adr - cur->start, val);
              }
            else
              {
                printf("WRITE @ 0x%04X <- %02X [ro/%s]\n", adr, val, cur->name);
              }
            return;
          }
        cur = cur->next;
      }

    printf("WRITE @ 0x%04X <- %02X [none]\n", adr, val);
  }

void hc11_core_map(struct hc11_core *core, const char *name, uint16_t start,
                   uint16_t count, void *ctx, read_f rd, write_f wr)
  {
    struct hc11_mapping *map = malloc(sizeof(struct hc11_mapping));
    struct hc11_mapping *cur, *next;

    map->next  = NULL;
    map->start = start;
    map->len   = count;
    map->ctx   = ctx;
    map->rdf   = rd;
    map->wrf   = wr;
    strncpy(map->name, name, sizeof(map->name));
    map->name[sizeof(map->name)-1] = 0;

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

void hc11_core_map_ram(struct hc11_core *core, const char *name, uint16_t start,
                          uint16_t count)
  {
    uint8_t *ram;
    ram = malloc(count);
    memset(ram,0,count);
    printf("Mapping %d bytes of RAM at address %04Xh\n", count, start);
    hc11_core_map(core, name, start, count, ram, ram_read, ram_write);
  }

void hc11_core_map_rom(struct hc11_core *core, const char *name, uint16_t start,
                       uint16_t count, uint8_t *rom)
  {
    hc11_core_map(core, name, start, count, rom, ram_read, NULL);
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

