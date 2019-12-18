#ifndef __core__h__
#define __core__h__

#include <stdint.h>

#define HC11_BKPT_NUM  8

enum hc11regs
  {
    REG_OPTION = 0x39,
    REG_HPRIO = 0x3C,
    REG_INIT = 0x3D,
  };

enum hc11vectors
  {
    VECTOR_SCI     = 0xFFD6,
    VECTOR_SPI     = 0xFFD8,
    VECTOR_PAIE    = 0xFFDA,
    VECTOR_PAIO    = 0xFFDC,
    VECTOR_TMRO    = 0xFFDE,
    VECTOR_OC5     = 0xFFE0,
    VECTOR_OC4     = 0xFFE2,
    VECTOR_OC3     = 0xFFE4,
    VECTOR_OC2     = 0xFFE6,
    VECTOR_OC1     = 0xFFE8,
    VECTOR_IC3     = 0xFFEA,
    VECTOR_IC2     = 0xFFEC,
    VECTOR_IC1     = 0xFFEE,
    VECTOR_RTI     = 0xFFF0,
    VECTOR_IRQ     = 0xFFF2,
    VECTOR_XIRQ    = 0xFFF4,
    VECTOR_SWI     = 0xFFF6,
    VECTOR_ILLEGAL = 0xFFF8,
    VECTOR_COPFAIL = 0xFFFA,
    VECTOR_CLCKMON = 0xFFFC,
    VECTOR_RESET   = 0xFFFE
  };

//for pulsel
enum
  {
  PULL_CCR,
  PULL_B,
  PULL_A,
  PULL_X,
  PULL_Y,
  PULL_PC
  };

enum
  {
    STATUS_STOPPED,
    STATUS_STEPPING,
    STATUS_RUNNING
  };

typedef uint8_t (*read_f )(void *ctx, uint16_t off);
typedef void    (*write_f)(void *ctx, uint16_t off, uint8_t val);

struct hc11_mapping
  {
    struct hc11_mapping *next;
    char     name[8];
    uint16_t start;
    uint16_t len;
    void     *ctx;
    read_f   rdf;
    write_f  wrf;    
  };

struct hc11_regs
  {
    uint16_t pc;
    uint16_t sp;
    uint16_t d;
    uint16_t x;
    uint16_t y;
    union
      {
        uint8_t  ccr;
        struct
          {
            uint8_t C : 1;
            uint8_t V : 1;
            uint8_t Z : 1;
            uint8_t N : 1;
            uint8_t H : 1;
            uint8_t I : 1;
            uint8_t X : 1;
            uint8_t S : 1;
          } flags;
      };
  };

struct hc11_io
  {
    void    *ctx;
    read_f  rdf;
    write_f wrf;
  };

struct hc11_core
  {
    struct hc11_regs     regs;
    struct hc11_io       io[64];
    uint8_t              iram[256];
    struct hc11_mapping *maps;    
    uint16_t             rambase;
    uint16_t             iobase;
    uint16_t             state; //core state machine
    uint64_t             clocks;
    volatile uint16_t    status; //stopped, stepping, running...
    uint16_t             break_pc[HC11_BKPT_NUM];
    // internal regs for execution
    uint16_t             busadr;
    uint16_t             busdat;
    uint8_t              prefix;
    uint8_t              opcode;
    uint8_t              addmode;
    uint16_t             operand;
    uint8_t              op2,op3, pulsel;
  };

void hc11_core_init(struct hc11_core *core);
void hc11_core_map(struct hc11_core *core, const char *name, uint16_t start,
                   uint16_t count, void *ctx, read_f rd, write_f wr);
void hc11_core_map_ram(struct hc11_core *core, const char *name, uint16_t start,
                       uint16_t count);
void hc11_core_map_rom(struct hc11_core *core, const char *name, uint16_t start,
                       uint16_t count, uint8_t *rom);
void hc11_core_iocallback(struct hc11_core *core, uint8_t off, uint8_t count,
                          void *ctx, read_f rd, write_f wr);

int hc11_core_set_bkpt(struct hc11_core *core, uint16_t pc);
int hc11_core_clr_bkpt(struct hc11_core *core, uint16_t pc);


uint8_t hc11_core_readb(struct hc11_core *core, uint16_t adr);
void    hc11_core_writeb(struct hc11_core *core, uint16_t adr,
                         uint8_t val);

void hc11_core_reset(struct hc11_core *core);
void hc11_core_clock(struct hc11_core *core);
void hc11_core_step (struct hc11_core *core);
void hc11_core_prep (struct hc11_core *core);

#endif /* __core__h__ */

