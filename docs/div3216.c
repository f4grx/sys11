//div3216.c
//demo and poc of hardware divide
//alg by fgr, thanks for help
#include <stdint.h>

uint16_t D;
uint16_t X;

uint8_t mem[16];

void LDX_EXT(uint16_t adr)
  {
  X = mem[adr]<< 8 | mem[adr+1];
  printf("LDX    %04X        X=%04X\n", adr, X);
  }

void LDD_EXT(uint16_t adr)
  {
  D = mem[adr]<< 8 | mem[adr+1];
  printf("LDD    %04X        D=%04X\n", adr, D);
  }

void LDX_IMM(uint16_t adr, uint16_t val)
  {
  X = val;
  printf("LDX    %04X        X=%04X\n", adr, X);
  }

void LDD_IMM(uint16_t adr, uint16_t val)
  {
  D = val;
  printf("LDD    %04X        D=%04X\n", adr, D);
  }

void STX_EXT(uint16_t adr)
  {
  mem[adr  ] = X >> 8;
  mem[adr+1] = X &  0xFF;
  printf("STX    %04X        X=%04X\n", adr, X);
  }

void STD_EXT(uint16_t adr)
  {
  mem[adr  ] = D >> 8;
  mem[adr+1] = D &  0xFF;
  printf("LDD    %04X        D=%04X\n", adr, D);
  }

void FDIV(void)
  {
  uint32_t dividend = D << 16;
  uint32_t divisor  = X;
  uint32_t quotient = dividend / divisor;
  uint32_t remainder = dividend % divisor;

  }

int main(void)
  {
  // setup input values for 12345678h / 5678h
  LDX_IMM(0x1234);
  STX_EXT(0);
  LDX_IMM(0x5678);
  STX_EXT(2);
  LDX_IMM(0x5678);
  STX_EXT(4);

  //main alg
  LDD_EXT(0);
  LDX_EXT(2);
  FDIV();
  }

