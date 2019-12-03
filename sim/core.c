#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "core.h"

// Define internal core execution states
enum hc11states
  {
    STATE_VECTORFETCH_H, //fetch high byte of vector address
    STATE_VECTORFETCH_L, //fetch low byte of vector address
    STATE_FETCHOPCODE,   //fetch the opcode or the prefix
    STATE_OPERAND_H,     //fetch operand (hi bytes)
    STATE_OPERAND_L,     //fetch operand (single or lo byte)
    STATE_READOP_H,      //read operand value (hi byte)
    STATE_READOP_L,      //read operand value (single or lo byte)
    STATE_EXECUTE,       //execute opcode
    STATE_EXECUTE_18,    //execute opcode with 18h prefix
    STATE_EXECUTE_1A,    //execute opcode with 1Ah prefix
    STATE_EXECUTE_CD,    //execute opcode with CDh prefix
    STATE_WRITEOP_H,     //write operand value (hi byte)
    STATE_WRITEOP_L,     //write operand value (single or lo byte)
  };

// Define addressing modes
enum
  {
    ILL, //illegal opcode
    INH, //inherent (no operand, direct execution
    IM1, //immediate, one byte
    IM2, //immediate, two bytes
    DIR, //direct (one byte abs address), value on 8 bits
    DI2, //direct, value on 16 bits (X,Y,D)
    REL, //relative (branches)
    EXT, //extended (two bytes absolute address), value on 8 buts
    EX2, //extended, value on 16 bits (X,Y,D)
    INX, //indexed relative to X, 8-bit value
    IX2, //indexed relative to X, 16-bit value
    INY, //indexed relative to Y, 8-bit value
    IY2, //indexed relative to Y, 16-bit value
    TDI, //direct, triple for brset/clr dd/mm/rr
    TIX, //indirect X, triple for brset/clr ff/mm/rr
    TIY, //indirect Y, triple for brset/clr ff/mm/rr
    DDI, //direct, double for bset/clr dd/mm/rr
    DIX, //indirect X, double for bset/clr ff/mm/rr
    DIY, //indirect Y, double for bset/clr ff/mm/rr
  };
//last 18ad
static const uint8_t opmodes[256] =
  {
/*00  01  02  03  04  05  06  07  08  09  0A  0B  0C  0D  0E  0F*/
  INH,INH,INH,INH,INH,INH,INH,INH,INH,INH,INH,0,  INH,INH,INH,INH, /* 00-0F */
  INH,INH,TDI,TDI,DDI,DDI,INH,INH,0,  INH,0,  INH,DIX,DIX,TIX,TIX, /* 10-1F */
  REL,REL,REL,REL,REL,REL,REL,REL,REL,REL,REL,REL,REL,REL,REL,REL, /* 20-2F */
  INH,INH,INH,INH,INH,INH,INH,INH,INH,INH,INH,INH,INH,INH,INH,INH, /* 30-3F */
  INH,0,  0,  INH,INH,0,  INH,INH,INH,INH,INH,0,  INH,INH,0,  INH, /* 40-4F */
  INH,0,  0,  INH,INH,0,  INH,INH,INH,INH,INH,0,  INH,INH,0,  INH, /* 50-5F */
  INX,0,  0,  INX,INX,0,  INX,INX,INX,INX,INX,0,  INX,INX,INX,INX, /* 60-5F */
  EXT,0,  0,  EXT,EXT,0,  EXT,EXT,EXT,EXT,EXT,0,  EXT,EXT,EXT,EXT, /* 70-7F */
  IM1,IM1,IM1,IM2,IM1,IM1,IM1,0,  IM1,IM1,IM1,IM1,IM2,REL,IM2,INH, /* 80-8F */
  DIR,DIR,DIR,DI2,DIR,DIR,DIR,DIR,DIR,DIR,DIR,DIR,DI2,DIR,DI2,DIR, /* 90-9F */
  INX,INX,INX,IX2,INX,INX,INX,INX,INX,INX,INX,INX,IX2,INX,IX2,INX, /* A0-AF */
  EXT,EXT,EXT,EX2,EXT,EXT,EXT,EXT,EXT,EXT,EXT,EXT,EX2,EXT,EX2,EXT, /* B0-BF */
  IM1,IM1,IM1,IM2,IM1,IM1,IM1,0,  IM1,IM1,IM1,IM1,IM2,0,  IM2,INH, /* C0-CF */
  DIR,DIR,DIR,DI2,DIR,DIR,DIR,DIR,DIR,DIR,DIR,DIR,DI2,DIR,DI2,DIR, /* D0-DF */
  INX,INX,INX,IX2,INX,INX,INX,INX,INX,INX,INX,INX,IX2,INX,IX2,INX, /* E0-EF */
  EXT,EXT,EXT,EX2,EXT,EXT,EXT,EXT,EXT,EXT,EXT,EXT,EX2,EXT,EX2,EXT, /* F0-FF */
  };

static const uint8_t opmodes_18[256] =
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
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  DI2,0,  0,  0, /* 90-9F */
  INY,INY,INY,IY2,INY,INY,INY,INY,INY,INY,INY,INY,IY2,INY,IY2,INY, /* A0-AF */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  EX2,0,  0,  0, /* B0-BF */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  IM2,0, /* C0-CF */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  DI2,DIR, /* D0-DF */
  INY,INY,INY,IY2,INY,INY,INY,INY,INY,INY,INY,INY,IY2,INY,IY2,INY, /* E0-EF */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  EX2,EXT, /* F0-FF */
  };

static const uint8_t opmodes_1A[256] =
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

static const uint8_t opmodes_CD[256] =
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

//Define opcodes
enum
  {
    OP_TEST_INH = 0x00,
    OP_NOP_INH  = 0x01,
    OP_IDIV_INH = 0x02,
    OP_FDIV_INH = 0x03,
    OP_LSRD_INH = 0x04,
    OP_ASLD_INH = 0x05,
    OP_TAP_INH  = 0x06,
    OP_TPA_INH  = 0x07,
    OP_INXY_INH = 0x08,
    OP_DEXY_INH = 0x09,
    OP_CLV_INH  = 0x0A,
    OP_SEV_INH  = 0x0B,
    OP_CLC_INH  = 0x0C,
    OP_SEC_INH  = 0x0D,
    OP_CLI_INH  = 0x0E,
    OP_SEI_INH  = 0x0F,

    OP_SBA_INH   = 0x10,
    OP_CBA_INH   = 0x11,
    OP_BRSET_TDI = 0x12,
    OP_BRCLR_TDI = 0x13,
    OP_BSET_DDI  = 0x14,
    OP_BCLR_DDI  = 0x15,
    OP_TAB_INH   = 0x16,
    OP_TBA_INH   = 0x17,
    OP_PFX_18    = 0x18,
    OP_DAA_INH   = 0x19,
    OP_PFX_1A    = 0x1A,
    OP_ABA_INH   = 0x1B,
    OP_BSET_DIN  = 0x1C,
    OP_BCLR_DIN  = 0x1D,
    OP_BRSET_TIN = 0x1E,
    OP_BRCLR_TIN = 0x1F,

    OP_BRA_REL  = 0x20,
    OP_BRN_REL  = 0x21,
    OP_BHI_REL  = 0x22,
    OP_BLS_REL  = 0x23,
    OP_BHS_REL  = 0x24,
    OP_BLO_REL  = 0x25,
    OP_BNE_REL  = 0x26,
    OP_BEQ_REL  = 0x27,
    OP_BVC_REL  = 0x28,
    OP_BVS_REL  = 0x29,
    OP_BPL_REL  = 0x2A,
    OP_BMI_REL  = 0x2B,
    OP_BGE_REL  = 0x2C,
    OP_BLT_REL  = 0x2D,
    OP_BGT_REL  = 0x2E,
    OP_BLE_REL  = 0x2F,

    OP_TSXY_INH  = 0x30,
    OP_INS_INH   = 0x31,
    OP_PULA_INH  = 0x32,
    OP_PULB_INH  = 0x33,
    OP_DES_INH   = 0x34,
    OP_TXYS_INH  = 0x35,
    OP_PSHA_INH  = 0x36,
    OP_PSHB_INH  = 0x37,
    OP_PULXY_INH = 0x38,
    OP_RTS_INH   = 0x39,
    OP_ABXY_INH  = 0x3A,
    OP_RTI_INH   = 0x3B,
    OP_PSHXY_INH = 0x3C,
    OP_MUL_INH   = 0x3D,
    OP_WAI_INH   = 0x3E,
    OP_SWI_INH   = 0x3F,

    OP_NEGA_INH = 0x40,
    OP_RSVD_41  = 0x41,
    OP_RSVD_42  = 0x42,
    OP_COMA_INH = 0x43,
    OP_LSRA_INH = 0x44,
    OP_RSVD_45  = 0x45,
    OP_RORA_INH = 0x46,
    OP_ASRA_INH = 0x47,
    OP_ASLA_INH = 0x48,
    OP_ROLA_INH = 0x49,
    OP_DECA_INH = 0x4A,
    OP_RSVD_4B  = 0x4B,
    OP_INCA_INH = 0x4C,
    OP_TSTA_INH = 0x4D,
    OP_RSVD_4E  = 0x4E,
    OP_CLRA_INH = 0x4F,

    OP_NEGB_INH = 0x50,
    OP_RSVD_51  = 0x51,
    OP_RSVD_52  = 0x52,
    OP_COMB_INH = 0x53,
    OP_LSRB_INH = 0x54,
    OP_RSVD_55  = 0x55,
    OP_RORB_INH = 0x56,
    OP_ASRB_INH = 0x57,
    OP_ASLB_INH = 0x58,
    OP_ROLB_INH = 0x59,
    OP_DECB_INH = 0x5A,
    OP_RSVD_5B  = 0x5B,
    OP_INCB_INH = 0x5C,
    OP_TSTB_INH = 0x5D,
    OP_RSVD_5E  = 0x5E,
    OP_CLRB_INH = 0x5F,

    OP_NEG_IND = 0x60,
    OP_RSVD_61 = 0x61,
    OP_RSVD_62 = 0x62,
    OP_COM_IND = 0x63,
    OP_LSR_IND = 0x64,
    OP_RSVD_65 = 0x65,
    OP_ROR_IND = 0x66,
    OP_ASR_IND = 0x67,
    OP_ASL_IND = 0x68,
    OP_ROL_IND = 0x69,
    OP_DEC_IND = 0x6A,
    OP_RSVD_6B = 0x6B,
    OP_INC_IND = 0x6C,
    OP_TST_IND = 0x6D,
    OP_JMP_IND = 0x6E,
    OP_CLR_IND = 0x6F,

    OP_NEG_EXT = 0x70,
    OP_RSVD_71 = 0x71,
    OP_RSVD_72 = 0x72,
    OP_COM_EXT = 0x73,
    OP_LSR_EXT = 0x74,
    OP_RSVD_75 = 0x75,
    OP_ROR_EXT = 0x76,
    OP_ASR_EXT = 0x77,
    OP_ASL_EXT = 0x78,
    OP_ROL_EXT = 0x79,
    OP_DEC_EXT = 0x7A,
    OP_RSVD_7B = 0x7B,
    OP_INC_EXT = 0x7C,
    OP_TST_EXT = 0x7D,
    OP_JMP_EXT = 0x7E,
    OP_CLR_EXT = 0x7F,

    OP_SUBA_IMM  = 0x80,
    OP_CMPA_IMM  = 0x81,
    OP_SBCA_IMM  = 0x82,
    OP_CPD_SUBD_IMM = 0x83,
    OP_ANDA_IMM  = 0x84,
    OP_BITA_IMM  = 0x85,
    OP_LDAA_IMM  = 0x86,
    OP_RSVD_87   = 0x87,
    OP_EORA_IMM  = 0x88,
    OP_ADCA_IMM  = 0x89,
    OP_ORAA_IMM  = 0x8A,
    OP_ADDA_IMM  = 0x8B,
    OP_CPXY_IMM  = 0x8C,
    OP_BSR_REL   = 0x8D,
    OP_LDS_IMM   = 0x8E,
    OP_XGDXY_INH = 0x8F,

    OP_SUBA_DIR = 0x90,
    OP_CMPA_DIR = 0x91,
    OP_SBCA_DIR = 0x92,
    OP_CPD_SUBD_DIR = 0x93,
    OP_ANDA_DIR = 0x94,
    OP_BITA_DIR = 0x95,
    OP_LDAA_DIR = 0x96,
    OP_STAA_DIR = 0x97,
    OP_EORA_DIR = 0x98,
    OP_ADCA_DIR = 0x99,
    OP_ORAA_DIR = 0x9A,
    OP_ADDA_DIR = 0x9B,
    OP_CPXY_DIR = 0x9C,
    OP_JSR_DIR  = 0x9D,
    OP_LDS_DIR  = 0x9E,
    OP_STS_DIR  = 0x9F,

    OP_SUBA_IND = 0xA0,
    OP_CMPA_IND = 0xA1,
    OP_SBCA_IND = 0xA2,
    OP_CPD_SUBD_IND = 0xA3,
    OP_ANDA_IND = 0xA4,
    OP_BITA_IND = 0xA5,
    OP_LDAA_IND = 0xA6,
    OP_STAA_IND = 0xA7,
    OP_EORA_IND = 0xA8,
    OP_ADCA_IND = 0xA9,
    OP_ORAA_IND = 0xAA,
    OP_ADDA_IND = 0xAB,
    OP_CPXY_IND = 0xAC,
    OP_JSR_IND  = 0xAD,
    OP_LDS_IND  = 0xAE,
    OP_STS_IND  = 0xAF,

    OP_SUBA_EXT = 0xB0,
    OP_CMPA_EXT = 0xB1,
    OP_SBCA_EXT = 0xB2,
    OP_CPD_SUBD_EXT = 0xB3,
    OP_ANDA_EXT = 0xB4,
    OP_BITA_EXT = 0xB5,
    OP_LDAA_EXT = 0xB6,
    OP_STAA_EXT = 0xB7,
    OP_EORA_EXT = 0xB8,
    OP_ADCA_EXT = 0xB9,
    OP_ORAA_EXT = 0xBA,
    OP_ADDA_EXT = 0xBB,
    OP_CPXY_EXT = 0xBC,
    OP_JSR_EXT  = 0xBD,
    OP_LDS_EXT  = 0xBE,
    OP_STS_EXT  = 0xBF,

    OP_SUBB_IMM = 0xC0,
    OP_CMPB_IMM = 0xC1,
    OP_SBCB_IMM = 0xC2,
    OP_ADDD_IMM = 0xC3,
    OP_ANDB_IMM = 0xC4,
    OP_BITB_IMM = 0xC5,
    OP_LDAB_IMM = 0xC6,
    OP_RSVD_C7  = 0xC7,
    OP_EORB_IMM = 0xC8,
    OP_ADCB_IMM = 0xC9,
    OP_ORAB_IMM = 0xCA,
    OP_ADDB_IMM = 0xCB,
    OP_LDD_IMM  = 0xCC,
    OP_PFX_CD   = 0xCD,
    OP_LDXY_IMM = 0xCE,
    OP_STOP_INH = 0xCF,

    OP_SUBB_DIR = 0xD0,
    OP_CMPB_DIR = 0xD1,
    OP_SBCB_DIR = 0xD2,
    OP_ADDD_DIR = 0xD3,
    OP_ANDB_DIR = 0xD4,
    OP_BITB_DIR = 0xD5,
    OP_LDAB_DIR = 0xD6,
    OP_STAB_DIR = 0xD7,
    OP_EORB_DIR = 0xD8,
    OP_ADCB_DIR = 0xD9,
    OP_ORAB_DIR = 0xDA,
    OP_ADDB_DIR = 0xDB,
    OP_LDD_DIR  = 0xDC,
    OP_STD_DIR  = 0xDD,
    OP_LDXY_DIR = 0xDE,
    OP_STXY_DIR = 0xDF,

    OP_SUBB_IND = 0xE0,
    OP_CMPB_IND = 0xE1,
    OP_SBCB_IND = 0xE2,
    OP_ADDD_IND = 0xE3,
    OP_ANDB_IND = 0xE4,
    OP_BITB_IND = 0xE5,
    OP_LDAB_IND = 0xE6,
    OP_STAB_IND = 0xE7,
    OP_EORB_IND = 0xE8,
    OP_ADCB_IND = 0xE9,
    OP_ORAB_IND = 0xEA,
    OP_ADDB_IND = 0xEB,
    OP_LDD_IND  = 0xEC,
    OP_STD_IND  = 0xED,
    OP_LDXY_IND = 0xEE,
    OP_STXY_IND = 0xEF,

    OP_SUBB_EXT = 0xF0,
    OP_CMPB_EXT = 0xF1,
    OP_SBCB_EXT = 0xF2,
    OP_ADDD_EXT = 0xF3,
    OP_ANDB_EXT = 0xF4,
    OP_BITB_EXT = 0xF5,
    OP_LDAB_EXT = 0xF6,
    OP_STAB_EXT = 0xF7,
    OP_EORB_EXT = 0xF8,
    OP_ADCB_EXT = 0xF9,
    OP_ORAB_EXT = 0xFA,
    OP_ADDB_EXT = 0xFB,
    OP_LDD_EXT  = 0xFC,
    OP_STD_EXT  = 0xFD,
    OP_LDXY_EXT = 0xFE,
    OP_STXY_EXT = 0xFF,
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

    printf("[%8ld] ", core->clocks); fflush(stdout);
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
    core->busadr  = VECTOR_RESET;
    core->state   = STATE_VECTORFETCH_H;
    core->prefix  = 0x00;
    core->clocks = 0;
  }

void hc11_core_clock(struct hc11_core *core)
  {
    core->clocks += 1;
    switch(core->state)
      {
        case STATE_VECTORFETCH_H:
          printf("VECTOR fetch @ 0x%04X\n", core->busadr);
          core->regs.pc = hc11_core_readb(core,core->busadr) << 8;
          core->state = STATE_VECTORFETCH_L;
          break;

        case STATE_VECTORFETCH_L:
          core->regs.pc |= hc11_core_readb(core,core->busadr+1);
          core->state = STATE_FETCHOPCODE;
          break;

        case STATE_FETCHOPCODE:
          core->busadr = core->regs.pc;
          core->busdat = hc11_core_readb(core,core->busadr);
          core->regs.pc = core->regs.pc + 1;           
          if(core->busdat == 0x18 || core->busdat == 0x1A || core->busdat == 0xCD)
            {
              if(core->prefix == 0)
                {
                  core->prefix = core->busdat;
                  break; //stay in this state
                }
              else
                {
                  //prefix already set: illegal
                  core->busadr  = VECTOR_ILLEGAL;
                  core->state   = STATE_VECTORFETCH_H;
                  break;
                }
            }
          else
            {
            //not a prefix
            const uint8_t *modtable = opmodes;
            core->opcode = core->busdat;
            if(core->prefix == 0x18) modtable = opmodes_18;
            if(core->prefix == 0x1A) modtable = opmodes_1A;
            if(core->prefix == 0xCD) modtable = opmodes_CD;
            core->addmode = modtable[core->opcode];
            printf("add mode: %d\n", core->addmode);
            switch(core->addmode)
              {
                case ILL: //illegal opcode
                  core->busadr  = VECTOR_ILLEGAL;
                  core->state   = STATE_VECTORFETCH_H;
                  break;

                case INH: //inherent (no operand, direct execution
                  core->state = STATE_EXECUTE; //actual next state depends on adressing mode
                  break;

                case IM1: //immediate, one byte
                case DIR: //direct (one byte abs address)
                case REL: //relative (branches)
                case INX: //indexed relative to X
                case INY: //indexed relative to Y
                  core->operand = 0;
                  core->state = STATE_OPERAND_L;
                  break;

                case IM2: //immediate, two bytes
                case EXT: //extended (two bytes absolute address)
                  core->state = STATE_OPERAND_H;
                  break;

                case TDI: //direct, triple for brset/clr dd/mm/rr
                case TIX: //indirect X, triple for brset/clr ff/mm/rr
                case TIY: //indirect Y, triple for brset/clr ff/mm/rr
                case DDI: //direct, double for bset/clr dd/mm
                case DIX: //indirect X, double for bset/clr ff/mm
                case DIY: //indirect Y, double for bset/clr ff/mm
                  printf("unsupported op mode\n");
                break;
              }
            }
          break;

        case STATE_OPERAND_H:
          core->busadr = core->regs.pc;
          core->busdat = hc11_core_readb(core,core->busadr);
          core->regs.pc = core->regs.pc + 1;
          core->operand = core->busdat << 8;
          core->state = STATE_OPERAND_L;
          break;

        case STATE_OPERAND_L:
          core->busadr = core->regs.pc;
          core->busdat = hc11_core_readb(core,core->busadr);
          core->regs.pc = core->regs.pc + 1;
          core->operand |= core->busdat;
          switch(core->addmode)
            {
              case DIR:
              case EXT:
                core->state = STATE_READOP_L;
                break;
              case DI2:
              case EX2:
                core->state = STATE_READOP_H;
                break;
              case INX:
                core->operand = core->regs.x + core->operand;
                core->state = STATE_READOP_L;
                break;
              case INY:
                core->operand = core->regs.y + core->operand;
                core->state = STATE_READOP_L;
                break;
              case IX2:
                core->operand = core->regs.x + core->operand;
                core->state = STATE_READOP_H;
                break;
              case IY2:
                core->operand = core->regs.y + core->operand;
                core->state = STATE_READOP_H;
                break;
            }

          core->state = STATE_EXECUTE;
          if(core->prefix == 0x18) core->state = STATE_EXECUTE_18;
          if(core->prefix == 0x1A) core->state = STATE_EXECUTE_1A;
          if(core->prefix == 0xCD) core->state = STATE_EXECUTE_CD;
          break;

        case STATE_READOP_H:
          core->busadr = core->operand >> 8;
          core->busdat = hc11_core_readb(core,core->busadr);
          core->busadr = core->busadr + 1;
          core->operand = (core->busdat<<8) | core->operand & 0xFF;
          core->state = STATE_READOP_L;
          break;

        case STATE_READOP_L:
          core->busadr = core->operand & 0xFF;
          core->busdat = hc11_core_readb(core,core->busadr);
          core->operand = (core->operand & 0xFF00) | core->busdat;
          core->state = STATE_EXECUTE;
          if(core->prefix == 0x18) core->state = STATE_EXECUTE_18;
          if(core->prefix == 0x1A) core->state = STATE_EXECUTE_1A;
          if(core->prefix == 0xCD) core->state = STATE_EXECUTE_CD;
          break;

        case STATE_EXECUTE:
          printf("STATE_EXECUTE op %02X operand %02X\n", core->opcode, core->operand);
          core->prefix = 0; //prepare for next opcode
          core->state = STATE_FETCHOPCODE; //default action when nothing needs writing

          switch(core->opcode)
            {
              case OP_TEST_INH :
                printf("TEST instruction not available in sim\n");
                break;

              case OP_NOP_INH  :
                printf("NOP\n");
                break;

              case OP_IDIV_INH : break;
              case OP_FDIV_INH : break;
              case OP_LSRD_INH : break;
              case OP_ASLD_INH : break;
              case OP_TAP_INH  : break;
              case OP_TPA_INH  : break;
              case OP_INXY_INH : break;
              case OP_DEXY_INH : break;
              case OP_CLV_INH  : break;
              case OP_SEV_INH  : break;
              case OP_CLC_INH  : break;
              case OP_SEC_INH  : break;
              case OP_CLI_INH  : break;
              case OP_SEI_INH  : break;

              case OP_SBA_INH   : break;
              case OP_CBA_INH   : break;
              case OP_BRSET_TDI : break;
              case OP_BRCLR_TDI : break;
              case OP_BSET_DDI  : break;
              case OP_BCLR_DDI  : break;
              case OP_TAB_INH   : break;
              case OP_TBA_INH   : break;
              case OP_DAA_INH   : break;
              case OP_ABA_INH   : break;
              case OP_BSET_DIN  : break;
              case OP_BCLR_DIN  : break;
              case OP_BRSET_TIN : break;
              case OP_BRCLR_TIN : break;

              case OP_BRA_REL  : break;
              case OP_BRN_REL  :
                printf("BRN\n");
                break;

              case OP_BHI_REL  : break;
              case OP_BLS_REL  : break;
              case OP_BHS_REL  : break;
              case OP_BLO_REL  : break;
              case OP_BNE_REL  : break;
              case OP_BEQ_REL  : break;
              case OP_BVC_REL  : break;
              case OP_BVS_REL  : break;
              case OP_BPL_REL  : break;
              case OP_BMI_REL  : break;
              case OP_BGE_REL  : break;
              case OP_BLT_REL  : break;
              case OP_BGT_REL  : break;
              case OP_BLE_REL  : break;

              case OP_TSXY_INH  : break;
              case OP_INS_INH   : break;
              case OP_PULA_INH  : break;
              case OP_PULB_INH  : break;
              case OP_DES_INH   : break;
              case OP_TXYS_INH  : break;
              case OP_PSHA_INH  : break;
              case OP_PSHB_INH  : break;
              case OP_PULXY_INH : break;
              case OP_RTS_INH   : break;
              case OP_ABXY_INH  : break;
              case OP_RTI_INH   : break;
              case OP_PSHXY_INH : break;
              case OP_MUL_INH   : break;
              case OP_WAI_INH   : break;
              case OP_SWI_INH   : break;

              case OP_NEGA_INH : break;
              case OP_COMA_INH : break;
              case OP_LSRA_INH : break;
              case OP_RORA_INH : break;
              case OP_ASRA_INH : break;
              case OP_ASLA_INH : break;
              case OP_ROLA_INH : break;
              case OP_DECA_INH : break;
              case OP_INCA_INH : break;
              case OP_TSTA_INH : break;
              case OP_CLRA_INH:
                core->regs.d = core->regs.d & 0x00FF;
                printf("CLRA\n");
                break;

              case OP_NEGB_INH : break;
              case OP_COMB_INH : break;
              case OP_LSRB_INH : break;
              case OP_RORB_INH : break;
              case OP_ASRB_INH : break;
              case OP_ASLB_INH : break;
              case OP_ROLB_INH : break;
              case OP_DECB_INH : break;
              case OP_INCB_INH : break;
              case OP_TSTB_INH : break;
              case OP_CLRB_INH:
                core->regs.d = core->regs.d & 0xFF00;
                printf("CLRB\n");
                break;

              case OP_NEG_IND : break;
              case OP_COM_IND : break;
              case OP_LSR_IND : break;
              case OP_ROR_IND : break;
              case OP_ASR_IND : break;
              case OP_ASL_IND : break;
              case OP_ROL_IND : break;
              case OP_DEC_IND : break;
              case OP_INC_IND : break;
              case OP_TST_IND : break;
              case OP_JMP_IND : break;
              case OP_CLR_IND : break;

              case OP_NEG_EXT : break;
              case OP_COM_EXT : break;
              case OP_LSR_EXT : break;
              case OP_ROR_EXT : break;
              case OP_ASR_EXT : break;
              case OP_ASL_EXT : break;
              case OP_ROL_EXT : break;
              case OP_DEC_EXT : break;
              case OP_INC_EXT : break;
              case OP_TST_EXT : break;
              case OP_JMP_EXT : break;
              case OP_CLR_EXT : break;

              case OP_SUBA_IMM  : break;
              case OP_CMPA_IMM  : break;
              case OP_SBCA_IMM  : break;
              case OP_CPD_SUBD_IMM : break;
              case OP_ANDA_IMM  : break;
              case OP_BITA_IMM  : break;
              case OP_LDAA_IMM  : break;
              case OP_EORA_IMM  : break;
              case OP_ADCA_IMM  : break;
              case OP_ORAA_IMM  : break;
              case OP_ADDA_IMM  : break;
              case OP_CPXY_IMM  : break;
              case OP_BSR_REL   : break;
              case OP_LDS_IMM   : break;
              case OP_XGDXY_INH : break;

              case OP_SUBA_IND : break;
              case OP_CMPA_IND : break;
              case OP_SBCA_IND : break;
              case OP_CPD_SUBD_IND : break;
              case OP_ANDA_IND : break;
              case OP_BITA_IND : break;
              case OP_LDAA_IND : break;
              case OP_STAA_IND : break;
              case OP_EORA_IND : break;
              case OP_ADCA_IND : break;
              case OP_ORAA_IND : break;
              case OP_ADDA_IND : break;
              case OP_CPXY_IND : break;
              case OP_JSR_IND  : break;
              case OP_LDS_IND  : break;
              case OP_STS_IND  : break;

              case OP_SUBA_DIR :
              case OP_SUBA_EXT : break;
              case OP_CMPA_DIR :
              case OP_CMPA_EXT : break;
              case OP_SBCA_DIR :
              case OP_SBCA_EXT : break;
              case OP_CPD_SUBD_DIR :
              case OP_CPD_SUBD_EXT : break;
              case OP_ANDA_DIR :
              case OP_ANDA_EXT : break;
              case OP_BITA_DIR :
              case OP_BITA_EXT : break;
              case OP_LDAA_DIR :
              case OP_LDAA_EXT : break;
              case OP_STAA_DIR :
              case OP_STAA_EXT :
                core->busadr = core->operand;
                core->busdat = core->regs.d >> 8;
                core->state = STATE_WRITEOP_L;
                printf("STAA_DIR_EXT\n");
                break;
              case OP_EORA_DIR :
              case OP_EORA_EXT : break;
              case OP_ADCA_DIR :
              case OP_ADCA_EXT : break;
              case OP_ORAA_DIR :
              case OP_ORAA_EXT : break;
              case OP_ADDA_DIR :
              case OP_ADDA_EXT : break;
              case OP_CPXY_DIR :
              case OP_CPXY_EXT : break;
              case OP_JSR_DIR  :
              case OP_JSR_EXT  : break;
              case OP_LDS_DIR  :
              case OP_LDS_EXT  : break;
              case OP_STS_DIR  :
              case OP_STS_EXT  : break;

    case OP_SUBB_IMM : break;
    case OP_CMPB_IMM : break;
    case OP_SBCB_IMM : break;
    case OP_ADDD_IMM : break;
    case OP_ANDB_IMM : break;
    case OP_BITB_IMM : break;
    case OP_LDAB_IMM : break;
    case OP_EORB_IMM : break;
    case OP_ADCB_IMM : break;
    case OP_ORAB_IMM : break;
    case OP_ADDB_IMM : break;
    case OP_LDD_IMM  : break;
    case OP_LDXY_IMM : break;
    case OP_STOP_INH : break;

    case OP_SUBB_DIR : break;
    case OP_CMPB_DIR : break;
    case OP_SBCB_DIR : break;
    case OP_ADDD_DIR : break;
    case OP_ANDB_DIR : break;
    case OP_BITB_DIR : break;
    case OP_LDAB_DIR : break;
    case OP_STAB_DIR : break;
    case OP_EORB_DIR : break;
    case OP_ADCB_DIR : break;
    case OP_ORAB_DIR : break;
    case OP_ADDB_DIR : break;
    case OP_LDD_DIR  : break;
    case OP_STD_DIR  : break;
    case OP_LDXY_DIR : break;
    case OP_STXY_DIR : break;

    case OP_SUBB_IND : break;
    case OP_CMPB_IND : break;
    case OP_SBCB_IND : break;
    case OP_ADDD_IND : break;
    case OP_ANDB_IND : break;
    case OP_BITB_IND : break;
    case OP_LDAB_IND : break;
    case OP_STAB_IND : break;
    case OP_EORB_IND : break;
    case OP_ADCB_IND : break;
    case OP_ORAB_IND : break;
    case OP_ADDB_IND : break;
    case OP_LDD_IND  : break;
    case OP_STD_IND  : break;
    case OP_LDXY_IND : break;
    case OP_STXY_IND : break;

    case OP_SUBB_EXT : break;
    case OP_CMPB_EXT : break;
    case OP_SBCB_EXT : break;
    case OP_ADDD_EXT : break;
    case OP_ANDB_EXT : break;
    case OP_BITB_EXT : break;
    case OP_LDAB_EXT : break;
    case OP_STAB_EXT : break;
    case OP_EORB_EXT : break;
    case OP_ADCB_EXT : break;
    case OP_ORAB_EXT : break;
    case OP_ADDB_EXT : break;
    case OP_LDD_EXT  : break;
    case OP_STD_EXT  : break;
    case OP_LDXY_EXT : break;
    case OP_STXY_EXT : break;

            } //normal opcodes
          break;
        case STATE_EXECUTE_18:
          printf("STATE_EXECUTE_18 op %02X operand %02X\n", core->opcode, core->operand);
          core->prefix = 0; //prepare for next opcode
          core->state = STATE_FETCHOPCODE; //default action when nothing needs writing
          switch(core->opcode)
            {
            }
            break;

        case STATE_EXECUTE_1A:
          printf("STATE_EXECUTE_1A op %02X operand %02X\n", core->opcode, core->operand);
          core->prefix = 0; //prepare for next opcode
          core->state = STATE_FETCHOPCODE; //default action when nothing needs writing
          switch(core->opcode)
            {
            }
            break;

        case STATE_EXECUTE_CD:
          printf("STATE_EXECUTE_CD op %02X operand %02X\n", core->opcode, core->operand);
          core->prefix = 0; //prepare for next opcode
          core->state = STATE_FETCHOPCODE; //default action when nothing needs writing
          switch(core->opcode)
            {
            }
            break;

        case STATE_WRITEOP_H:
          hc11_core_writeb(core,core->busadr, core->busdat >> 8);
          core->busadr = core->busadr + 1;
          core->state = STATE_WRITEOP_L;
          break;

        case STATE_WRITEOP_L:
          hc11_core_writeb(core,core->busadr, core->busdat);
          core->state = STATE_FETCHOPCODE;
          break;

      }//switch
  }

