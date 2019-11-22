#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MEMSIZE 512
uint8_t memory[MEMSIZE];
uint16_t head;

#define SIZE 0
#define NEXT 2
#define EOL  0xFFFF

#define ADDR(ptr) (uint16_t)((uint8_t*)(ptr)-memory)
#define PTR(addr) (uint8_t*)((uint16_t)(addr)+memory)

/* ************************************************************************* */
static inline void POKE_U16BE(uint16_t addr, uint16_t val)
  {
    if(addr >= MEMSIZE)
      {
        printf("ERROR trying to store at addr %04X\n",addr);
        return;
      }
    memory[addr+0] = val>>8;
    memory[addr+1] = val&0xFF;
  }

/* ************************************************************************* */
static inline uint16_t PEEK_U16BE(uint16_t addr)
  {
    if(addr >= MEMSIZE)
      {
        printf("ERROR trying to load at addr %04X\n",addr);
        return 0xFFFF;
      }
    return (((uint16_t)memory[addr]) << 8) | ((uint16_t)memory[addr+1]);
  }

/* ************************************************************************* */
void mem_init(void)
  {
  POKE_U16BE(SIZE, MEMSIZE-2);
  POKE_U16BE(NEXT, EOL);
  head = 0;
  }

/* ************************************************************************* */
uint16_t mem_split(uint16_t adr, uint16_t size)
  {
    uint16_t nxtadr = adr + 2 + size;
    POKE_U16BE(nxtadr+SIZE, PEEK_U16BE(adr+SIZE) - 2 - size);
    POKE_U16BE(nxtadr+NEXT, PEEK_U16BE(adr+NEXT));
    //Update params of current block
    POKE_U16BE(adr+SIZE, size);
    POKE_U16BE(adr+NEXT, nxtadr);
    //Now link
    //if(adr == head)
      //head = nxtadr;
    return adr;
  }

/* ************************************************************************* */
void mem_status(void)
  {
    uint16_t adr;
    printf("mem status\n");
    for(adr=0; adr < MEMSIZE; adr+=2)
      {
        if( (adr & 0x1F) == 0x00)
          printf("%04X: ",adr);
        printf("%04X ", PEEK_U16BE(adr));
        if( (adr & 0x1F) == 0x1E)
          printf("\n");
      }
    printf("  META    SIZE   NEXT\n");
    adr = head;
    while(adr != EOL)
      {
        uint16_t s = PEEK_U16BE(adr+SIZE);
        uint16_t n = PEEK_U16BE(adr+NEXT);
        printf("0x%04X: 0x%04X 0x%04X\n", adr,s,n);
        adr = n;
      }
    printf("mem status end\n");
  }

/* ************************************************************************* */
uint16_t mem_alloc(uint16_t size)
  {
    uint16_t adr = head;
    uint16_t nxt;
    printf("\nasked to alloc %u bytes\n",size);
    while(adr != EOL)
      {
        uint16_t n = PEEK_U16BE(adr+NEXT);
        uint16_t s = PEEK_U16BE(adr+SIZE);
        printf("meta at %04X: next %04X size %04X\n", adr,n,s);
        if(s > size)
          {
            printf("suitable but bigger -> split\n");
            adr = mem_split(adr,size);
            s   = size;
          }
        if(s == size)
          {
            printf("match\n");
            //Remove from free list
            nxt = PEEK_U16BE(adr+NEXT);
            if(adr == head)
              head = nxt;
            POKE_U16BE(adr+SIZE, size);
            return adr+2;
          }
        adr = n;
      }
    printf("no suitable room found\n");
    return 0;
  }

/* ************************************************************************* */
void mem_coalesce(void)
  {
    uint16_t siz;
    uint16_t adr;
    uint16_t nxt;
again:
    adr = head;
    while(adr != EOL)
      {
        siz = PEEK_U16BE(adr+SIZE);
        nxt = PEEK_U16BE(adr+NEXT);
        printf("coalesce_check: adr %04X size %04X after %04X next %04X\n", adr,siz, (adr+siz+2), nxt);
        if(nxt == adr + siz + 2)
          {
            printf("blocks %04X and %04X can be joined\n", adr, nxt);
            siz += PEEK_U16BE(nxt+SIZE) + 2;
            POKE_U16BE(adr+SIZE, siz);
            nxt = PEEK_U16BE(nxt+NEXT);
            POKE_U16BE(adr+NEXT, nxt);
            mem_status();
            goto again;
          }
        adr = nxt;
      }
    printf("mem_coalesce done\n");
  }

/* ************************************************************************* */
void mem_free(uint16_t cur)
  {
    uint16_t siz = PEEK_U16BE(cur-2);
    uint16_t adr;
    uint16_t nxt;
    cur -= 2;
    printf("\nasked to free adr 0x%04X, block size %u (0x%04X)\n", cur, siz, siz);
    //Find insertion point
    if(cur < head)
      {
        //new head
        POKE_U16BE(cur+NEXT, head);
        head = cur;
      }
    else
      {
        //browse free blocks until we find the insertion point
        adr = head;
        while(adr != EOL)
          {
            nxt = PEEK_U16BE(adr+NEXT);
            if(nxt > cur)
              break;
            adr = nxt;
          }
        if(adr == EOL)
          {
            printf("alg error :(\n");
            return;
          }
        printf("insert to freelist: curhdr=%04X adr=%04X nxt=%04X\n",cur,adr,nxt);
        POKE_U16BE(cur+NEXT, nxt);
        POKE_U16BE(adr+NEXT, cur);
      }
    mem_coalesce();
  }

/* ************************************************************************* */
int main(int argc, char **argv)
  {
  uint16_t adr1,adr2,adr3;
  mem_init();
  adr1 = mem_alloc(10); printf("got adr 0x%04X\n",adr1);
  mem_status();
  adr2 = mem_alloc(20); printf("got adr 0x%04X\n",adr2);
  mem_status();
  mem_free(adr1);
  mem_status();
  adr1 = mem_alloc(10); printf("got adr 0x%04X\n",adr1);
  mem_status();
  mem_free(adr1);
  mem_status();
  mem_free(adr2);
  mem_status();
  printf("//should have one big block at this point\n");
 
  adr1 = mem_alloc(10); printf("got adr 0x%04X\n",adr1);
  mem_status();
  adr2 = mem_alloc(10); printf("got adr 0x%04X\n",adr2);
  mem_status();
  adr3 = mem_alloc(10); printf("got adr 0x%04X\n",adr1);
  mem_status();
  mem_free(adr2); //no coalescence possible
  mem_status();
  mem_free(adr1); //coalesce two first ranges
  mem_status(); //should have two free zones here

  return 0;
  }

