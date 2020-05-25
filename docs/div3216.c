//div3216.c
//demo and poc of hardware divide
//alg by fgr, thanks for help
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

uint16_t D;
uint16_t X;
uint16_t C;

uint8_t mem[256];

void CLRA(void)
  {
  D = D & 0x00FF;
  }

void CLRB(void)
  {
  D = D & 0xFF00;
  }

void LDX_EXT(uint16_t adr)
  {
  X = mem[adr]<< 8 | mem[adr+1];
  printf("LDX    $%04X        X=$%04X (%u)\n", adr, X, X);
  }

void LDD_EXT(uint16_t adr)
  {
  D = mem[adr]<< 8 | mem[adr+1];
  printf("LDD    $%04X        D=$%04X (%u)\n", adr, D, D);
  }

void LDX_IMM(uint16_t val)
  {
  X = val;
  printf("LDX    #$%04X (%u)\n", X, X);
  }

void LDD_IMM(uint16_t val)
  {
  D = val;
  printf("LDD    #$%04X (%u)\n", D, D);
  }

void STX_EXT(uint16_t adr)
  {
  mem[adr  ] = X >> 8;
  mem[adr+1] = X &  0xFF;
  printf("STX    $%04X        X=$%04X (%u)\n", adr, X, X);
  }

void STD_EXT(uint16_t adr)
  {
  mem[adr  ] = D >> 8;
  mem[adr+1] = D &  0xFF;
  printf("STD    $%04X        D=$%04X (%u)\n", adr, D, D);
  }

void ADDD_EXT(uint16_t adr)
  {
  uint16_t val = mem[adr]<< 8 | mem[adr+1];
  uint32_t sum = D + val;
  D = sum & 0xFFFF;
  C = sum >> 16;
  printf("ADDD   $%04X        D=$%04X (%u)\n", adr, D, D);
  }

void RLCB(void)
  {
  D = ( D & 0xFF00 ) | ((D&0x7F) << 1) | C;
  printf("RLCB   $%04X\n", D);
  }

void FDIV(void) // D<<16/X -> Q in X, R in D
  {
  uint32_t dividend = D << 16;
  uint32_t divisor  = X;
  uint32_t quotient = dividend / divisor;
  uint32_t remainder = dividend % divisor;

  printf("FDIV    D=$%04X    X=$%04X\n"
         "    dividend = $%08X\n"
         "    divisor  = $%08X\n"
         "    quotient = $%08X\n"
         "    remainder= $%08X\n"
         ,D,X,dividend,divisor,quotient,remainder
        );
  if(quotient > 0xFFFF)
    {
    printf("    *** quotient overflow\n");
    quotient = 0xFFFF;
    }

  X = quotient;
  D = remainder;
  }

void IDIV(void) // D<<16/X -> Q in X, R in D
  {
  uint16_t dividend = D;
  uint16_t divisor  = X;
  uint16_t quotient = dividend / divisor;
  uint16_t remainder = dividend % divisor;

  printf("IDIV    D=$%04X    X=$%04X\n"
         "    dividend = $%04X\n"
         "    divisor  = $%04X\n"
         "    quotient = $%04X\n"
         "    remainder= $%04X\n"
         ,D,X,dividend,divisor,quotient,remainder
        );
  X = quotient;
  D = remainder;
  }

int main(int argc, char **argv)
  {
#define H0 0
#define L0 2
#define DD 4
#define Q0 6
#define R0 8
#define H1 10
#define L1 12
#define Q1 14
#define R1 16
#define L2 18
#define Q  24
#define R  26

  uint32_t num = 0x12345678;
  uint16_t den = 0x9ABC;

  if(argc==3)
    {
    num = strtoul(argv[1], NULL, 0);
    den = strtoul(argv[2], NULL, 0) & 0xFFFF;
    }

  // setup input values for 12345678h / 5678h
  printf("INPUT NUMERATOR = $%08X DENOMINATOR = $%04X\n", num, den);

  LDX_IMM(num>>16);
  STX_EXT(H0);
  LDX_IMM(num&0xFFFF);
  STX_EXT(L0);
  LDX_IMM(den);
  STX_EXT(DD);

  printf("----------------\n");
  //main alg
  LDD_EXT(H0);
  LDX_EXT(DD);
  FDIV();
  STX_EXT(Q0);
  STD_EXT(R0);

  printf("----------------\n");
  ADDD_EXT(L0);
  STD_EXT(L1);
  CLRA();
  CLRB();
  RLCB();
  STD_EXT(H1);

  printf("----------------\n");
  LDD_EXT(H1);
  LDX_EXT(DD);
  FDIV();
  STX_EXT(Q1);
  STD_EXT(R1);

  printf("----------------\n");
  ADDD_EXT(L1);
  STD_EXT(L2);

  printf("----------------\n");
  LDD_EXT(L2);
  LDX_EXT(DD);
  IDIV();
  STX_EXT(Q);
  STD_EXT(R);

  LDD_EXT(Q0);
  ADDD_EXT(Q1);
  ADDD_EXT(Q);
  LDX_EXT(R);

  printf("OUTPUT Q=$%04X (%u) R=$%04X (%u)\n", D, D, X, X);
  }

