#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MEMSTART 0x100
#define MEMSIZE 256
uint8_t memory[65536];
uint16_t head;

#define SIZE 0
#define NEXT 2
#define EOL  0xFFFF

#define ADDR(ptr) (uint16_t)((uint8_t*)(ptr)-memory)
#define PTR(addr) (uint8_t*)((uint16_t)(addr)+memory)

/* ************************************************************************* */
static inline void POKE_U16BE(uint16_t addr, uint16_t val)
  {
    memory[addr+0] = val>>8;
    memory[addr+1] = val&0xFF;
  }

/* ************************************************************************* */
static inline uint16_t PEEK_U16BE(uint16_t addr)
  {
    return (((uint16_t)memory[addr]) << 8) | ((uint16_t)memory[addr+1]);
  }

/* ************************************************************************* */
void mem_init(void)
  {
  POKE_U16BE(MEMSTART+SIZE, MEMSIZE-2);
  POKE_U16BE(MEMSTART+NEXT, EOL);
  head = MEMSTART;
  }

/* ************************************************************************* */
void mem_status(void)
  {
    uint16_t adr;
    printf("mem status\n");
    for(adr=MEMSTART; adr < MEMSTART+MEMSIZE; adr+=2)
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
    uint16_t nxtadr;
    uint16_t prvadr;
    uint16_t tmp;
    uint16_t cursiz;
    printf("\nasked to alloc %u bytes\n",size);
    prvadr = EOL;
    while(adr != EOL)
      {
        cursiz = PEEK_U16BE(adr+SIZE);
        printf("check @%04X (sz %04X, %d)\n", adr, cursiz, cursiz);
        if(cursiz > size)
          {
            nxtadr = adr + 2 + size;
            printf("suitable but bigger -> split, newfree=%04X\n", nxtadr);

            tmp = PEEK_U16BE(adr+SIZE) - 2 - size;
            POKE_U16BE(nxtadr+SIZE, tmp);

            tmp = PEEK_U16BE(adr+NEXT);
            POKE_U16BE(nxtadr+NEXT, tmp);

            //Update params of current block
            POKE_U16BE(adr+NEXT, nxtadr);
            POKE_U16BE(adr+SIZE, size);

            cursiz = size;
          }
        nxtadr = PEEK_U16BE(adr+NEXT);
        if(cursiz == size)
          {
            printf("match, adr=%04X\n", adr);
            //Remove from free list
            if(adr == head)
              {
                head = nxtadr; //the head of list now look at the second block
              }
            else
              {
              //the allocated block was free in the middle of the list.
              //next of previous is now next of current.
              POKE_U16BE(prvadr+NEXT , nxtadr);
              }
            return adr+NEXT;
          }
        prvadr = adr;
        adr = nxtadr;
      }
    printf("no suitable room found\n");
    return 0;
  }

/* ************************************************************************* */
void mem_free(uint16_t adr)
  {
    uint16_t siz;
    uint16_t cur;
    uint16_t nxt;
    adr -= 2;
    siz = PEEK_U16BE(adr);
    printf("\nasked to free cur 0x%04X, block size %u (0x%04X)\n", adr, siz, siz);
    //Find insertion point
    if(adr < head)
      {
        //easy: this block becomes the new head, the old head is now next of new head
        POKE_U16BE(adr+NEXT, head);
        head = adr;
      }
    else
      {
        //browse free blocks until we find the insertion point: cur < adr < nxt
        cur = head;
        while(cur != EOL)
          {
            nxt = PEEK_U16BE(cur+NEXT);
            if(nxt > adr)
              {
                break;
              }
            cur = nxt;
          }
        printf("insert to freelist: adrhdr=%04X cur=%04X nxt=%04X\n",adr,cur,nxt);
        POKE_U16BE(adr+NEXT, nxt);
        POKE_U16BE(cur+NEXT, adr);
      }

    //now coalesce, brutal but compact technique. trying again if anything happens.
again:
    cur = head;
    while(cur != EOL)
      {
        siz = PEEK_U16BE(cur+SIZE);
        nxt = PEEK_U16BE(cur+NEXT);
        printf("coalesce_check: cur %04X size %04X after %04X next %04X\n", cur,siz, (cur+siz+2), nxt);
        if(nxt == cur + siz + 2)
          {
            printf("blocks %04X and %04X can be joined\n", cur, nxt);
            siz += PEEK_U16BE(nxt+SIZE) + 2;
            POKE_U16BE(cur+SIZE, siz);
            nxt = PEEK_U16BE(nxt+NEXT);
            POKE_U16BE(cur+NEXT, nxt);
            //mem_status();
            goto again;
          }
        cur = nxt;
      }
    printf("mem_coalesce done\n");
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
  adr3 = mem_alloc(10); printf("got adr 0x%04X\n",adr3);
  mem_status();
  mem_free(adr2); //no coalescence possible
  mem_status();
  mem_free(adr1); //coalesce two first ranges
  mem_status();
  printf("//should have two free zones here\n");

  adr1 = mem_alloc(10); printf("got adr 0x%04X\n",adr1);
  mem_status();
  adr2 = mem_alloc(10); printf("got adr 0x%04X\n",adr2);
  mem_status();
  adr1 = mem_alloc(10); printf("got adr 0x%04X\n",adr1);
  mem_status();
  adr3 = mem_alloc(20); printf("got adr 0x%04X\n",adr3);
  mem_status();
  adr1 = mem_alloc(10); printf("got adr 0x%04X\n",adr1);
  mem_status();
  mem_free(adr2); //no coalescence possible
  mem_free(adr3); //no coalescence possible
  mem_status();
  adr3 = mem_alloc(20); printf("got adr 0x%04X\n",adr3);
  mem_status();

  return 0;
  }

