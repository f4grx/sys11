#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define EQU(var, val) uint16_t var = val;

	EQU(RAMSTART, 0x0100)
	//EQU(RAMLEN  , 0x8000 - 0x0100)
	EQU(RAMLEN  , 256)

#define SIZE 0
#define NEXT 2
#define EOL  0xFFFF

struct {
  uint8_t N:1;
  uint8_t Z:1;
  uint8_t C:1;
  uint8_t V:1;
} flags;

void calc_flags_16(uint16_t a, uint16_t b)
  {
//N=R7
//Z=!R0&!R1&!R2&!R3&!R4&!R5&!R6&!R7
//V=A7&!B7&!R7+!A7&B7&R7
//C=!A7&B7+B7&R7+R7&!A7

  int16_t res = a - b;
  flags.Z = (res == 0);
  flags.N = (res >> 15);
  flags.C = (!(a>>15)&&(b>>15))||((b>>15)&&(res>>15))|((res>>15)&&!(a>>15));
  flags.V = ((a>>15)&&!(b>>15)&&!(res>>15))+(!(a>>15)&&(b>>15)&&(res>>15));
  }

uint16_t X,Y,D,comp;
#define RTS()            return D;
#define PULX(val)        X = (val);
#define LDX_IMM(val)     X = (val);

#define LDD_IMM(val)     D = (val);
#define LDD_DIR(adr)     D = PEEK_U16BE(adr);
#define LDD_IND(off,reg) D = PEEK_U16BE(off+reg);

#define LDX_DIR(adr)     X = PEEK_U16BE(adr);
#define LDX_IND(off,reg) X = PEEK_U16BE(off+reg);

#define LDY_DIR(adr)     Y = PEEK_U16BE(adr);

#define STD_IND(off,reg) POKE_U16BE(off+reg, D);
#define STD_DIR(adr)     POKE_U16BE(adr, D);

#define STX_DIR(adr)     POKE_U16BE(adr, X);

#define CPX_IMM(val)     calc_flags_16(X,(val));
#define CPX_DIR(adr)     calc_flags_16(X,PEEK_U16BE(adr));

#define CPD_DIR(adr)     calc_flags_16(D,PEEK_U16BE(adr));

#define BRA(lbl)         goto lbl;
#define BEQ(lbl)         if(flags.Z) goto lbl;
#define BNE(lbl)         if(!flags.Z) goto lbl;
#define BLO(lbl)         if(flags.C) goto lbl;
#define BHS(lbl)         if(!flags.C) goto lbl;
#define BHI(lbl)         if(!(flags.Z || flags.C)) goto lbl;

#define ADDD_IMM(val)    D = D + (val);
#define ADDD_DIR(adr)    D = D + PEEK_U16BE(adr);

#define SUBD_IMM(val)    D = D - (val);
#define SUBD_DIR(adr)    D = D - PEEK_U16BE(adr);

#define CLRA()           D = D & 0xFF00;
#define CLRB()           D = D & 0x00FF;
#define INX()            X = X + 1;
#define DEX()            X = X - 1;
#define XGDX()           do { uint16_t tmp = X; X = D; D = tmp; } while(0);
#define XGDY()           do { uint16_t tmp = Y; Y = D; D = tmp; } while(0);

uint8_t mem[65536];

//addresses of vars
#define head 0x40
#define sr0  0x42
#define sr1  0x44
#define sr2  0x46
#define sr3  0x48
 
/* ************************************************************************* */
static inline void POKE_U16BE(uint16_t addr, uint16_t val)
  {
    mem[addr+0] = val>>8;
    mem[addr+1] = val&0xFF;
  }

/* ************************************************************************* */
static inline uint16_t PEEK_U16BE(uint16_t addr)
  {
    return (((uint16_t)mem[addr]) << 8) | ((uint16_t)mem[addr+1]);
  }

/* ************************************************************************* */
void mem_init(void)
  {
	LDX_IMM(	RAMSTART)
	LDD_IMM(	RAMLEN-2)	/* Block size = all but two header bytes */
	STD_IND(	0,X)
	LDD_IMM(	0xffff)
	STD_IND(	2,X)		/* This is the pointer to next (none) */
	STX_DIR(	head)		/* Initialize head of free zone list */
	RTS()
  }

/* ************************************************************************* */
void mem_status(void)
  {
    uint16_t adr;
    printf("mem status\n");
    for(adr=RAMSTART; adr < RAMSTART+RAMLEN; adr+=2)
      {
        if( (adr & 0x1F) == 0x00)
          printf("%04X: ",adr);
        printf("%04X ", PEEK_U16BE(adr));
        if( (adr & 0x1F) == 0x1E)
          printf("\n");
      }
    printf("  META    SIZE   NEXT\n");
    adr = PEEK_U16BE(head);
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
	/* Get requested size from stack into *sr2 */
	PULX(size)
	STX_DIR(	sr2)	/* *sr2 <- size */

	/* Browse all zones in the free list.
	 * We need one that is larger than the requested size
	 */
	LDX_DIR(	head)
	STX_DIR(	sr3)	/* *sr3 <- adr */
Lagain:
	LDX_DIR(	sr3)	/* X <- adr */
	LDX_IND(	0,X)	/* X <- cursiz */
	STX_DIR(	sr0)	/* *sr0 <- cursiz */
	CPX_DIR(	sr2)	/* Compare cursiz and size */
	BLO(		Lnext)	/* Current free block smaller than request? try next */
	/* At this point we have a zone that is big enough for allocation */
	BEQ(		Lalloc)	/* Zone has the correct size */
	/* Zone is larger than requested, we have to do a split */	
Lsplit:
	LDD_DIR(	sr2)	/* D <- size */
	ADDD_IMM(	2)	/* D <- size + 2 */
	ADDD_DIR(	sr3)	/* D <- size + 2 + adr == nxtadr */
	STD_DIR(	sr1)	/* *sr1 <- nxtadr */

	LDX_DIR(	sr3)	/* X <- adr */
	LDD_IND(	0,X)	/* D <- PEEK(adr+SIZE) */
	SUBD_IMM(	2)	/* D <- PEEK(adr+SIZE) - 2 */
	SUBD_DIR(	sr2)	/* D <- PEEK(adr+SIZE) - 2 - size == tmp */
	LDX_DIR(	sr1)	/* X <- nxtadr */
	STD_IND(	0,X)	/* POKE(nxtadr+SIZE, tmp)*/

	LDX_DIR(	sr3)	/* X <- adr */
	LDD_IND(	2,X)	/* D <- PEEK(adr+NEXT) == tmp */
	LDX_DIR(	sr1)	/* X <- nxtadr */
	STD_IND(	2,X)	/* POKE(nxtadr+NEXT, tmp) */

	LDX_DIR(	sr3)	/* X contain adr */
	LDD_DIR(	sr1)	/* D <- nxtadr */
	STD_IND(	2,X)	/* POKE adr+NEXT, nxtadr */
	LDD_DIR(	sr2)	/* D <- size */
	STD_IND(	0,X)	/* POKE adr+SIZE, size */

	/* now the current free zone @*sr3 has the correct size */

Lalloc:
	/* Set the mem fields to allocate this zone */
	LDX_DIR(	sr3)		/* X <- adr */
	CPX_DIR(	head)		/* is cur zone (alloced) the head of list? */
	BNE(		Lretblock)	/* no: we can now return the block */
	/* yes: so the head is the zone after the allocated block */
	LDD_DIR(	sr1) 		/* Get next zone pointer */
	STD_DIR(	head)		/* Update head */
Lretblock:
	INX()			/* X still has *sr3(adr), skip size */
	INX()			/* skip size */
	LDD_IMM(0)
	STD_IND(0,X)
	XGDX()			/* put in D, X gets garbage */
	BRA(	Lend)		/* We're done! */

Lnext:	/* Setup pointers to look at next block */
	LDX_DIR(	sr1)	/* Get curzone->next */
	CPX_IMM(	0xffff)	/* Is next the end of the list? */
	BEQ(		Ldone)	/* Yes, alloc was not possible, were done */
	STX_DIR(	sr3)	/* now current zone is next zone */
	BRA(		Lagain)	/* Try to fit the request in the next zone */
Ldone:
	/* We reached the end of free zones without finding a big enough one */
	/* We have to fail the allocation by returning NULL */
	CLRA()	/* DL <- 0 */
	CLRB()	/* DH <- 0 */
Lend:
	RTS()
  }

/* ************************************************************************* */
void mem_free(uint16_t adr)
  {
mm_free:
	PULX(adr)		/* X <- pointer to data to free */
	DEX()		/* Get pointer to zone */
	DEX()		/* X <- adr */
	STX_DIR(	sr0)	/* sr0 <- adr */
	CPX_DIR(	head)	/* Compare adr with head */
	BHS(		Lbrowse)
	/* adr is < head, replace head */
	LDD_DIR(	head)	/* D <- head */
	STD_IND(	2,X)	/* POKE(adr+NEXT, head) */
	STX_DIR(	head)	/* head <- adr */
	BRA(		Lcoalesce)

	/* Browse free list to find insertion point */
Lbrowse:
	LDX_DIR(	head)	/* X == cur <- head */
Lbrowsenext:
	CPX_IMM(	0xFFFF)	/* Check end of list */
	BEQ(		Leol)
	LDD_IND(	2,X)	/* D == nxt <- PEEK(cur+NEXT) */
	CPD_DIR(	sr0)
	BHI(		Leol)	/* break if next > adr, D has nxt, X has cur */
	XGDX()		/* X == cur <-> D == nxt : cur = nxt*/
	BRA(		Lbrowsenext)
Leol:
	/* Insert into free list */
	STD_IND(	2,X)	/* POKE(cur+NEXT, nxt) */
	XGDX()		/* D <- cur */
	LDX_DIR(	sr0)	/* X <- adr */
	STD_IND(	2,X)	/* POKE(adr+NEXT, cur) */

	/* Coalesce - merge block if current block finishes right before next block (cur+size+2==next) */
Lcoalesce:
	LDX_DIR(	head)	/* X == cur <- head */
Lcoalloop:
	CPX_IMM(	0xFFFF)	/* end of list reached? */
	BEQ(		Lcoaldone)
	STX_DIR(	sr2)	/* sr2 <- cur (has to be saved for use later in block merge) */
	LDD_IND(	2,X)
	STD_DIR(	sr1)	/*sr1 == D <- nxt == PEEK(cur+NEXT) */
	LDD_IND(	0,X)
	STD_DIR(	sr0)	/*sr0 == D <- siz == PEEK(cur+SIZE) */
	INX()
	INX()		/* Double inc X saves one byte wrt addd #0x0002 */
	XGDX()		/* D <- cur + 2 */
	ADDD_DIR(	sr0)	/* D <- cur + 2 + siz */
	CPD_DIR(	sr1)	/* Compare with next */
	BNE(		Lcoalnext)
Lcoaldo:
	/* free blocks adr and nxt are adjacent, can be merged */
	LDX_DIR(	sr1)	/* X <- nxt */
	LDD_IND(	2,X)	/* D <- PEEK(nxt+SIZE) */
	ADDD_IMM(	2)	/* D <- PEEK(nxt+SIZE) + 2 */
	ADDD_DIR(	sr0)	/* D <- siz + nxtsiz + 2 */
	LDX_DIR(	sr2)	/* X <- cur */
	STD_IND(	0,X)	/* POKE(cur+SIZE, D) */
	LDY_DIR(	sr1)	/* Y <- nxt */
	LDD_IND(	2,Y)	/* D == nxt = PEEK(nxt+NEXT) */
        STD_IND(	2,X)	/* POKE(cur+NEXT, nxt) */
	BRA(		Lcoalesce)	/* Try again */
Lcoalnext:
	/* These blocks are not adjacent, replace cur by next and try again for next pair */
	LDX_DIR(	sr1)
	BRA(		Lcoalloop)
Lcoaldone:
	RTS()
}


/* ************************************************************************* */
int main(int argc, char **argv)
  {
  uint16_t adr1,adr2,adr3;
  mem_init();
  printf("ask 10 bytes\n"); adr1 = mem_alloc(10); printf("got adr 0x%04X\n",adr1);
  mem_status();
  printf("ask 20 bytes\n"); adr2 = mem_alloc(20); printf("got adr 0x%04X\n",adr2);
  mem_status();
  printf("release at %04X\n", adr1); mem_free(adr1);
  mem_status();
  printf("ask 10 bytes\n"); adr1 = mem_alloc(10); printf("got adr 0x%04X\n",adr1);
  mem_status();
  printf("release at %04X\n", adr1); mem_free(adr1);
  mem_status();
  printf("release at %04X\n", adr2); mem_free(adr2);
  mem_status();
  printf("//should have one big block at this point\n");
 
  printf("ask 10 bytes\n"); adr1 = mem_alloc(10); printf("got adr 0x%04X\n",adr1);
  mem_status();
  printf("ask 10 bytes\n"); adr2 = mem_alloc(10); printf("got adr 0x%04X\n",adr2);
  mem_status();
  printf("ask 10 bytes\n"); adr3 = mem_alloc(10); printf("got adr 0x%04X\n",adr1);
  mem_status();
  printf("release at %04X\n", adr2); mem_free(adr2); //no coalescence possible
  mem_status();
  printf("release at %04X\n", adr1); mem_free(adr1); //coalesce two first ranges
  mem_status(); //should have two free zones here

  return 0;
  }
