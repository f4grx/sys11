#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include "core.h"

// Define internal core execution states
enum hc11states
  {
    STATE_VECTORFETCH_H,  //fetch high byte of vector address
    STATE_VECTORFETCH_L,  //fetch low byte of vector address
    STATE_FETCHOPCODE,    //fetch the opcode or the prefix
    STATE_OPERAND_H,      //fetch operand (hi bytes)
    STATE_OPERAND_L,      //fetch operand (single or lo byte)
    STATE_OPERAND_DOUBLE, //Special mode for BSET/BCLR
    STATE_OPERAND_TRIPLE, //Special mode for BRCLR
    STATE_READOP_H,       //read operand value (hi byte)
    STATE_READOP_L,       //read operand value (single or lo byte)
    STATE_RDMASK,
    STATE_RDREL,
    STATE_EXECUTE,        //execute opcode
    STATE_EXECUTE_18,     //execute opcode with 18h prefix
    STATE_EXECUTE_1A,     //execute opcode with 1Ah prefix
    STATE_EXECUTE_CD,     //execute opcode with CDh prefix
    STATE_EXECUTENEXT,
    STATE_WRITEOP_H,      //write operand value (hi byte)
    STATE_WRITEOP_L,      //write operand value (single or lo byte)
    STATE_PUSH_L,
    STATE_PUSH_H,
    STATE_PULL_L,
    STATE_PULL_H,
  };

// Define addressing modes
enum
  {
    ILL, //illegal opcode
    INH, //inherent (no operand, direct execution
    IM1, //immediate, one byte
    IM2, //immediate, two bytes
    DIR, //direct (one byte abs address), read 8 bits of data
    DI2, //direct (one byte abs address), read 16 bits of data (X,Y,D)
    DIS, //direct (one byte abs address), no read of data (store only)
    REL, //relative (branches)
    EXT, //extended (two bytes absolute address), read 8 bits of data
    EX2, //extended (two bytes absolute address), read 16 bits of data (X,Y,D)
    EXS, //extended (two bytes absolute address), no read of data (store only)
    INX, //indexed relative to X, 8-bit value
    IX2, //indexed relative to X, 16-bit value
    IXS, //indexed relative to X, no value read
    INY, //indexed relative to Y, 8-bit value
    IY2, //indexed relative to Y, 16-bit value
    IYS, //indexed relative to Y, no value read
  };
//last 18ad
static const uint8_t opmodes[256] =
  {
/*00  01  02  03  04  05  06  07  08  09  0A  0B  0C  0D  0E  0F*/
  INH,INH,INH,INH,INH,INH,INH,INH,INH,INH,INH,0  ,INH,INH,INH,INH, /* 00-0F */
  INH,INH,DIR,DIR,DIR,DIR,INH,INH,0  ,INH,0  ,INH,INX,INX,INX,INX, /* 10-1F */
  REL,REL,REL,REL,REL,REL,REL,REL,REL,REL,REL,REL,REL,REL,REL,REL, /* 20-2F */
  INH,INH,INH,INH,INH,INH,INH,INH,INH,INH,INH,INH,INH,INH,INH,INH, /* 30-3F */
  INH,0  ,0  ,INH,INH,0  ,INH,INH,INH,INH,INH,0  ,INH,INH,0  ,INH, /* 40-4F */
  INH,0  ,0  ,INH,INH,0  ,INH,INH,INH,INH,INH,0  ,INH,INH,0  ,INH, /* 50-5F */
  INX,0  ,0  ,INX,INX,0  ,INX,INX,INX,INX,INX,0  ,INX,INX,IXS,INX, /* 60-6F */
  EXT,0  ,0  ,EXT,EXT,0  ,EXT,EXT,EXT,EXT,EXT,0  ,EXT,EXT,EXS,EXT, /* 70-7F */
  IM1,IM1,IM1,IM2,IM1,IM1,IM1,0  ,IM1,IM1,IM1,IM1,IM2,REL,IM2,INH, /* 80-8F */
  DIR,DIR,DIR,DI2,DIR,DIR,DIR,DIS,DIR,DIR,DIR,DIR,DI2,DIR,DI2,DIS, /* 90-9F */
  INX,INX,INX,IX2,INX,INX,INX,IXS,INX,INX,INX,INX,IX2,INX,IX2,IXS, /* A0-AF */
  EXT,EXT,EXT,EX2,EXT,EXT,EXT,EXS,EXT,EXT,EXT,EXT,EX2,EXT,EX2,EXS, /* B0-BF */
  IM1,IM1,IM1,IM2,IM1,IM1,IM1,0  ,IM1,IM1,IM1,IM1,IM2,0,  IM2,INH, /* C0-CF */
  DIR,DIR,DIR,DI2,DIR,DIR,DIR,DIS,DIR,DIR,DIR,DIR,DI2,DIS,DI2,DIS, /* D0-DF */
  INX,INX,INX,IX2,INX,INX,INX,IXS,INX,INX,INX,INX,IX2,IXS,IX2,IXS, /* E0-EF */
  EXT,EXT,EXT,EX2,EXT,EXT,EXT,EXS,EXT,EXT,EXT,EXT,EX2,EXS,EX2,EXS, /* F0-FF */
  };

static const uint8_t opmodes_18[256] =
  {
/*00  01  02  03  04  05  06  07  08  09  0A  0B  0C  0D  0E  0F*/
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,INH,INH,0  ,0  ,0  ,0  ,0  ,0  , /* 00-0F */
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,INY,INY,INY,INY, /* 10-1F */
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  , /* 20-2F */
  INH,0  ,0  ,0  ,0  ,INH,0  ,0  ,INH,0  ,INH,0  ,INH,0  ,0  ,0  , /* 30-3F */
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  , /* 40-4F */
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  , /* 50-5F */
  INY,0  ,0  ,INY,INY,0  ,INY,INY,INY,INY,INY,0  ,INY,INY,IYS,INY, /* 60-6F */
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  , /* 70-7F */
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,IM2,0  ,0  ,INH, /* 80-8F */
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,DI2,0  ,0  ,0  , /* 90-9F */
  INY,INY,INY,IY2,INY,INY,INY,IYS,INY,INY,INY,INY,IY2,INY,IY2,IYS, /* A0-AF */
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,EX2,0  ,0  ,0  , /* B0-BF */
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,IM2,0  , /* C0-CF */
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,DI2,DIS, /* D0-DF */
  INY,INY,INY,IY2,INY,INY,INY,IYS,INY,INY,INY,INY,IY2,IYS,IY2,IYS, /* E0-EF */
  0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,EX2,EXS, /* F0-FF */
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
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 60-6F */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 70-7F */
  0,  0,  0,  IM2,0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 80-8F */
  0,  0,  0,  DIR,0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 90-9F */
  0,  0,  0,  INX,0,  0,  0,  0,  0,  0,  0,  0,  INX,0,  0,  0, /* A0-AF */
  0,  0,  0,  EXT,0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* B0-BF */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* C0-CF */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* D0-DF */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  IX2,IXS, /* E0-EF */
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
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 60-6F */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 70-7F */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 80-8F */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* 90-9F */
  0,  0,  0,  INY,0,  0,  0,  0,  0,  0,  0,  0,  INY,0,  0,  0, /* A0-AF */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* B0-BF */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* C0-CF */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* D0-DF */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  IY2,IYS, /* E0-EF */
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* F0-FF */
  };

//Define opcodes
enum
  {
    OP_TEST_INH  = 0x00, OP_NOP_INH, OP_IDIV_INH, OP_FDIV_INH, OP_LSRD_INH, OP_ASLD_INH, OP_TAP_INH, OP_TPA_INH,
    OP_INXY_INH  = 0x08, OP_DEXY_INH, OP_CLV_INH, OP_SEV_INH, OP_CLC_INH, OP_SEC_INH, OP_CLI_INH, OP_SEI_INH,
    OP_SBA_INH   = 0x10, OP_CBA_INH, OP_BRSET_DIR, OP_BRCLR_DIR, OP_BSET_DIR, OP_BCLR_DIR, OP_TAB_INH, OP_TBA_INH,
    OP_PFX_18    = 0x18, OP_DAA_INH, OP_PFX_1A, OP_ABA_INH, OP_BSET_IND, OP_BCLR_IND, OP_BRSET_IND, OP_BRCLR_IND,
    OP_BRA_REL   = 0x20, OP_BRN_REL, OP_BHI_REL, OP_BLS_REL, OP_BHS_REL, OP_BLO_REL, OP_BNE_REL, OP_BEQ_REL,
    OP_BVC_REL   = 0x28, OP_BVS_REL, OP_BPL_REL, OP_BMI_REL, OP_BGE_REL, OP_BLT_REL, OP_BGT_REL, OP_BLE_REL,
    OP_TSXY_INH  = 0x30, OP_INS_INH, OP_PULA_INH, OP_PULB_INH, OP_DES_INH, OP_TXYS_INH, OP_PSHA_INH, OP_PSHB_INH,
    OP_PULXY_INH = 0x38, OP_RTS_INH, OP_ABXY_INH, OP_RTI_INH, OP_PSHXY_INH, OP_MUL_INH, OP_WAI_INH, OP_SWI_INH,
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

uint8_t init_read(void *ctx, uint16_t off)
  {
    struct hc11_core *core = ctx;
    return ((core->rambase >> 12) << 4) | (core->iobase >> 12);
  }

void init_write(void *ctx, uint16_t off, uint8_t val)
  {
    struct hc11_core *core = ctx;
    core->rambase = (val >> 4   ) << 12;
    core->iobase  = (val &  0x0F) << 12;
    printf("INIT: rambase %04X iobase %04X\n", core->rambase, core->iobase);
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
    hc11_core_iocallback(core, REG_INIT, 1, core, init_read, init_write);
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
          printf("----------------------------------------\n");
          printf("VECTOR fetch @ 0x%04X\n", core->busadr);
          core->regs.pc = hc11_core_readb(core,core->busadr) << 8;
          core->state = STATE_VECTORFETCH_L;
          break;

        case STATE_VECTORFETCH_L:
          core->regs.pc |= hc11_core_readb(core,core->busadr+1);
          core->state = STATE_FETCHOPCODE;
          break;

        case STATE_FETCHOPCODE:
          printf("----------------------------------------\n");
          core->busadr = core->regs.pc;
          core->busdat = hc11_core_readb(core,core->busadr);
          core->regs.pc = core->regs.pc + 1;
          core->operand = 0;         
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
            //printf("add mode: %d\n", core->addmode);
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
                case DIS: //direct (one byte abs address, no data fetched)
                case REL: //relative (branches)
                case INX: //indexed relative to X
                case INY: //indexed relative to Y
                case IX2: //indexed relative to X, two bytes fetch
                case IY2: //indexed relative to Y, two bytes fetch
                case IXS: //indexed relative to X, no fetch
                case IYS: //indexed relative to Y, no fetch
                  core->state = STATE_OPERAND_L;
                  break;

                case IM2: //immediate, two bytes
                case EXT: //extended (two bytes absolute address)
                case EX2: //extended (two bytes absolute address, 2 bytes fetch)
                case EXS: //extended (two bytes absolute address, no data fetch)
                  core->state = STATE_OPERAND_H;
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
          core->busdat = 0;
          core->state = STATE_EXECUTE; //preset action to just execute without operand value fetch
          //fetch operand value if we need to act on it during the exec phase
          // this is not required for stores and jmps.
          switch(core->addmode)
            {
              case DIR:
              case EXT:
                core->state = STATE_READOP_L; //read value not used for jsr and bsr, but still acquired
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
          core->busadr = core->operand;
          if(core->state != STATE_EXECUTE)
            {
              break; //Something to do before execution
            }
          if(core->prefix == 0x18) core->state = STATE_EXECUTE_18;
          if(core->prefix == 0x1A) core->state = STATE_EXECUTE_1A;
          if(core->prefix == 0xCD) core->state = STATE_EXECUTE_CD;
          break;

        case STATE_READOP_H: //Get value in busdat (not operand, required for writeback)
          core->busdat = hc11_core_readb(core,core->busadr) << 8;
          core->busadr = core->busadr + 1;
          core->state = STATE_READOP_L;
          break;

        case STATE_READOP_L:
          core->busdat |= (uint16_t)hc11_core_readb(core,core->busadr);
          core->state = STATE_EXECUTE;
          if(core->prefix == 0x18) core->state = STATE_EXECUTE_18;
          if(core->prefix == 0x1A) core->state = STATE_EXECUTE_1A;
          if(core->prefix == 0xCD) core->state = STATE_EXECUTE_CD;
          break;

        case STATE_RDMASK:
          core->busadr = core->regs.pc;
          core->op2 = hc11_core_readb(core,core->busadr) & 0xFF;
          core->regs.pc = core->regs.pc + 1;
          if(core->opcode == OP_BRSET_DIR || core->opcode == OP_BRCLR_DIR ||
             core->opcode == OP_BRSET_IND || core->opcode == OP_BRCLR_IND)
            {
              core->state = STATE_RDREL;
            }
          else
            {
              core->state = STATE_EXECUTENEXT;
            }

          break;

        case STATE_RDREL:
          core->busadr = core->regs.pc;
          core->op3 = hc11_core_readb(core,core->busadr) & 0xFF;
          core->regs.pc = core->regs.pc + 1;
          core->state = STATE_EXECUTENEXT;
          break;

        case STATE_EXECUTENEXT: //finish BRSET/BRCLR insns
          core->state = STATE_FETCHOPCODE; //default action when nothing needs writing
          printf("[%8ld] EXEC_NEXT\n",core->clocks);
          switch(core->opcode)
            {
              uint16_t tmp;
              int16_t  rel;

              case OP_BSET_DIR:
              case OP_BSET_IND:
                core->busadr = core->operand;
                core->busdat = core->busdat | core->op2;
                core->state = STATE_WRITEOP_L;
                tmp = core->busdat & 0xFF;
                core->regs.flags.N = tmp >> 7;
                core->regs.flags.Z = tmp == 0;
                core->regs.flags.V = 0;
                printf("BSET MASK %02X\n", core->op2);
                break;

              case OP_BCLR_DIR:
              case OP_BCLR_IND:
                core->busadr = core->operand;
                core->busdat = core->busdat & (!core->op2);
                core->state = STATE_WRITEOP_L;
                tmp = core->busdat & 0xFF;
                core->regs.flags.N = tmp >> 7;
                core->regs.flags.Z = tmp == 0;
                core->regs.flags.V = 0;
                printf("BCLR MASK %02X\n", core->op2);
                break;

              case OP_BRSET_DIR:
              case OP_BRSET_IND:
                tmp = (~(core->busdat) & core->op2) & 0xFF;
                if(!tmp)
                  {
                    rel = (int16_t)((int8_t)core->op3);
                    core->regs.pc = core->regs.pc + rel;
                  }
                printf("BRSET MASK %02X REL %02X PC %04X\n", core->op2, core->op3, core->regs.pc);
                break;

              case OP_BRCLR_DIR:
              case OP_BRCLR_IND:
                tmp = (core->busdat & core->op2) & 0xFF;
                if(!tmp)
                  {
                    rel = (int16_t)((int8_t)core->op3);
                    core->regs.pc = core->regs.pc + rel;
                  }
                printf("BRCLR MASK %02X REL %02X PC %04X\n", core->op2, core->op3, core->regs.pc);
                break;
            }
          break;

        case STATE_EXECUTE:
          printf("[%8ld] EXEC  %02X operand %04X\n", core->clocks, core->opcode, core->operand);
          core->prefix = 0; //prepare for next opcode
          core->state = STATE_FETCHOPCODE; //default action when nothing needs writing

          switch(core->opcode)
            {
              uint16_t tmp;
              int16_t  rel;
              case OP_TEST_INH :
                printf("TEST instruction not available in sim\n");
                break;

              case OP_NOP_INH  :
                printf("NOP\n");
                break;

              case OP_IDIV_INH : /*ZVC*/ break;
              case OP_FDIV_INH : /*ZVC*/ break;
              case OP_MUL_INH   : /*C*/ break;

              case OP_TAP_INH  : /*SXHINZVC*/ break;
              case OP_TPA_INH  : break;

              case OP_TAB_INH   : /*NZV*/
                tmp = core->regs.d >> 8; //get A
                core->regs.d = (core->regs.d & 0xFF00) | tmp;
                core->regs.flags.N = tmp >> 7;
                core->regs.flags.Z = tmp == 0;
                core->regs.flags.V = 0;
                printf("TAB\n");
                break;

              case OP_TBA_INH   : /*NZV*/
                tmp = core->regs.d & 0xFF; //get B
                core->regs.d = (core->regs.d & 0x00FF) | (tmp << 8);
                core->regs.flags.N = tmp >> 7;
                core->regs.flags.Z = tmp == 0;
                core->regs.flags.V = 0;
                printf("TBA\n");
                break;


              case OP_CLV_INH  : /*V*/ break;
              case OP_SEV_INH  : /*V*/ break;
              case OP_CLC_INH  : /*C*/ break;
              case OP_SEC_INH  : /*C*/ break;
              case OP_CLI_INH  : /*I*/ break;
              case OP_SEI_INH  : /*I*/ break;

              case OP_ABXY_INH  : break;

              case OP_ABA_INH   : /*HNZCV*/ break;
              case OP_SBA_INH   : /*NZVC*/ break;
              case OP_CBA_INH   : /*NZVC*/ break;
              case OP_DAA_INH   : /*NZC*/ break;

              case OP_CLRA_INH : /*NZVC*/
                core->regs.d = core->regs.d & 0x00FF;
                core->regs.flags.N = 0;
                core->regs.flags.Z = 1;
                core->regs.flags.V = 0;
                core->regs.flags.C = 0;
                printf("CLRA\n");
                break;

              case OP_CLRB_INH : /*NZVC*/
                core->regs.d = core->regs.d & 0xFF00;
                core->regs.flags.N = 0;
                core->regs.flags.Z = 1;
                core->regs.flags.V = 0;
                core->regs.flags.C = 0;
                printf("CLRB\n");
                break;

              case OP_INCA_INH : /*NZV*/ break;
              case OP_INCB_INH : /*NZV*/ break;
              case OP_DECA_INH : /*NZV*/ break;
              case OP_DECB_INH : /*NZV*/ break;
              case OP_LSRA_INH : /*NZVC*/ break;
              case OP_LSRB_INH : /*NZVC*/ break;
              case OP_RORA_INH : /*NZVC*/ break;
              case OP_RORB_INH : /*NZVC*/ break;
              case OP_ASRA_INH : /*NZVC*/ break;
              case OP_ASRB_INH : /*NZVC*/ break;
              case OP_ASLA_INH : /*NZVC*/ break;
              case OP_ASLB_INH : /*NZVC*/ break;
              case OP_ROLA_INH : /*NZVC*/ break;
              case OP_ROLB_INH : /*NZVC*/ break;
              case OP_NEGA_INH : /*NZVC*/ break;
              case OP_NEGB_INH : /*NZVC*/ break;
              case OP_COMA_INH : /*NZVC*/ break;
              case OP_COMB_INH : /*NZVC*/ break;
              case OP_TSTA_INH : /*NZVC*/ break;
              case OP_TSTB_INH : /*NZVC*/ break;

              case OP_PSHA_INH  :
                core->busdat = (core->regs.d >> 8) << 8;
                core->state = STATE_PUSH_H; // single wordm positioned in MSByte
                printf("PSHA\n");
                break;

              case OP_PSHB_INH  :
                core->busdat = (core->regs.d & 0xFF) << 8;
                core->state = STATE_PUSH_H; // single word, positioned in MSByte
                printf("PSHB\n");
                break;

              case OP_PULA_INH  :
                core->pulsel = PULL_A;
                core->state = STATE_PULL_L;
                printf("PULX\n");
                break;

              case OP_PULB_INH  :
                core->pulsel = PULL_B;
                core->state = STATE_PULL_L;
                printf("PULX\n");
                break;

              case OP_LSRD_INH : /*NZVC*/ break;
              case OP_ASLD_INH : /*NZVC*/ break;

              case OP_TSXY_INH  : break;
              case OP_TXYS_INH  : break;
              case OP_INS_INH   : break;
              case OP_DES_INH   : break;

              case OP_INXY_INH : /*Z*/
                core->regs.x = core->regs.x + 1;
                core->regs.flags.Z = (core->regs.x == 0);
                printf("INX -> %04X\n", core->regs.x );
                break;

              case OP_DEXY_INH : /*Z*/
                core->regs.x = core->regs.x - 1;
                core->regs.flags.Z = (core->regs.x == 0);
                printf("DEX -> %04X\n", core->regs.x );
                break;

              case OP_PSHXY_INH :
                core->busdat = core->regs.x;
                core->state = STATE_PUSH_L; // not H, push happens L first
                printf("PSHX\n");
                break;

              case OP_PULXY_INH :
                core->pulsel = PULL_X;
                core->state = STATE_PULL_H;
                printf("PULX\n");
                break;

              case OP_RTS_INH:
                core->pulsel = PULL_PC;
                core->state = STATE_PULL_H;
                printf("RTS\n");
                break;

              case OP_RTI_INH   : /*SXHINZVC*/ break;

              case OP_WAI_INH   : break;
              case OP_SWI_INH   : break;


              case OP_BRSET_IND :
              case OP_BRSET_DIR :
                printf("BRSET_DIR_IND %04X\n", core->operand);
                core->state = STATE_RDMASK;
                break;

              case OP_BRCLR_IND :
              case OP_BRCLR_DIR :
                printf("BRCLR_DIR_IND %04X\n", core->operand);
                core->state = STATE_RDMASK;
                break;

              case OP_BSET_DIR  : /*NZV*/
              case OP_BSET_IND  :
                printf("BSET_DIR_IND %04X\n", core->operand);
                core->state = STATE_RDMASK;
                break;

              case OP_BCLR_DIR  : /*NZV*/
              case OP_BCLR_IND  :
                printf("BCLR_DIR_IND %04X\n", core->operand);
                core->state = STATE_RDMASK;
                break;


              case OP_BRA_REL  :
                rel = (int16_t)((int8_t)core->operand);
                core->regs.pc = core->regs.pc + rel;
                printf("BRA %04X\n", core->regs.pc);
                break;

              case OP_BRN_REL  :
                printf("BRN\n");
                break;

              case OP_BHI_REL  : break;
              case OP_BLS_REL  : break;
              case OP_BHS_REL  : break;
              case OP_BLO_REL  : break;

              case OP_BNE_REL  : break;
                if(!core->regs.flags.Z)
                  {
                  rel = (int16_t)((int8_t)core->operand);
                  core->regs.pc = core->regs.pc + rel;
                  }
                printf("BNE -> Z=%d pc=%04X\n" , core->regs.flags.Z, core->regs.pc);
                break;


              case OP_BEQ_REL  :
                if(core->regs.flags.Z)
                  {
                  rel = (int16_t)((int8_t)core->operand);
                  core->regs.pc = core->regs.pc + rel;
                  }
                printf("BEQ -> Z=%d pc=%04X\n" , core->regs.flags.Z, core->regs.pc);
                break;

              case OP_BVC_REL  : break;
              case OP_BVS_REL  : break;
              case OP_BPL_REL  : break;
              case OP_BMI_REL  : break;
              case OP_BGE_REL  : break;
              case OP_BLT_REL  : break;
              case OP_BGT_REL  : break;
              case OP_BLE_REL  : break;

              case OP_BSR_REL  :
                rel = (int16_t)((int8_t)core->operand);
                core->busdat = core->regs.pc;
                core->regs.pc = core->regs.pc + rel;
                core->state = STATE_PUSH_L; // not H, push happens L first
                printf("BSR %04X\n", core->regs.pc);
                break;

              case OP_NEG_EXT : /*NZVC*/
              case OP_NEG_IND : break;

              case OP_COM_EXT : /*NZVC*/
              case OP_COM_IND : break;

              case OP_LSR_EXT : /*NZVC*/
              case OP_LSR_IND : break;

              case OP_ROR_EXT : /*NZVC*/
              case OP_ROR_IND : break;

              case OP_ASR_EXT : /*NZVC*/
              case OP_ASR_IND : break;

              case OP_ASL_EXT : /*NZVC*/
              case OP_ASL_IND : break;

              case OP_ROL_EXT :/*NZVC*/
              case OP_ROL_IND : break;

              case OP_DEC_EXT : /*NZV*/
              case OP_DEC_IND : break;

              case OP_INC_EXT : /*NZV*/
              case OP_INC_IND : break;

              case OP_TST_EXT : /*NZVC*/
              case OP_TST_IND : break;

              case OP_JMP_EXT :
              case OP_JMP_IND : break;

              case OP_CLR_EXT :/*NZVC*/
              case OP_CLR_IND :
                core->regs.flags.N = 0;
                core->regs.flags.Z = 1;
                core->regs.flags.V = 0;
                core->regs.flags.C = 0;
                core->busadr = core->operand;
                core->busdat = 0;
                core->state = STATE_WRITEOP_L;
                printf("CLR_DIR_EXT_IND\n");
                break;

              case OP_SUBA_IMM : /*NZVC*/ break;
              case OP_CMPA_IMM : /*NZVC*/break;
              case OP_SBCA_IMM : /*NZVC*/ break;
              case OP_CPD_SUBD_IMM : /* SUBD: NZVC*/ break;
              case OP_ANDA_IMM : /*NZV*/ break;
              case OP_BITA_IMM : /*NZV*/ break;
              case OP_LDAA_IMM : /*NZV*/
                core->regs.d = (core->regs.d & 0x00FF) | (core->operand & 0xFF) << 8;
                tmp = core->regs.d >> 8;
                core->regs.flags.Z = tmp == 0;
                core->regs.flags.N = tmp >> 7;
                core->regs.flags.V = 0;
                printf("LDAA_IMM #%02X\n", core->operand & 0xFF);
                break;

              case OP_EORA_IMM  : /*NZV*/ break;
              case OP_ADCA_IMM  : /*HNZVC*/ break;
              case OP_ORAA_IMM  : /*NZV*/ break;
              case OP_ADDA_IMM  : /*HNZVC*/ break;
              case OP_CPXY_IMM  : /*NZVC*/ break;
              case OP_LDS_IMM   : /*NZV*/
                core->regs.sp = core->operand;
                printf("LDS_IMM %04X\n", core->operand);
                break;
              case OP_SUBB_IMM : /*NZVC*/break;
              case OP_CMPB_IMM : /*NZVC*/break;
              case OP_SBCB_IMM : /*NZVC*/break;
              case OP_ADDD_IMM : /*NZVC*/break;
              case OP_ANDB_IMM : /*NZV*/ break;
              case OP_BITB_IMM : /*NZV*/ break;
              case OP_LDAB_IMM :/*NZV*/
                core->regs.d = (core->regs.d & 0xFF00) | (core->operand & 0xFF);
                tmp = core->regs.d && 0xFF;
                core->regs.flags.Z = tmp == 0;
                core->regs.flags.N = tmp >> 7;
                core->regs.flags.V = 0;
                printf("LDAB_IMM #%02X\n", core->operand & 0xFF);
                break;

              case OP_EORB_IMM : /*NZV*/break;
              case OP_ADCB_IMM : /*HNZVC*/ break;
              case OP_ORAB_IMM : /*NZV*/ break;
              case OP_ADDB_IMM : /*HNZVC*/break;
              case OP_LDD_IMM  :/*NZV*/ 
                core->regs.d = core->operand;
                printf("LDD_IMM #%04X\n", core->operand);
                break;

              case OP_LDXY_IMM : /*NZV*/
                core->regs.x = core->operand;
                printf("LDX_IMM #%04X\n", core->operand);
                break;

              case OP_XGDXY_INH : break;


              case OP_SUBA_IND :/*NZVC*/
              case OP_SUBA_DIR :
              case OP_SUBA_EXT : break;
              case OP_CMPA_IND :/*NZVC*/
              case OP_CMPA_DIR :
              case OP_CMPA_EXT : break;
              case OP_SBCA_IND :/*NZVC*/
              case OP_SBCA_DIR :
              case OP_SBCA_EXT : break;
              case OP_CPD_SUBD_IND :/*subd: NZVC*/
              case OP_CPD_SUBD_DIR :
              case OP_CPD_SUBD_EXT : break;
              case OP_ANDA_IND :/*NZV*/ 
              case OP_ANDA_DIR :
              case OP_ANDA_EXT : break;
              case OP_BITA_IND :/*NZV*/ 
              case OP_BITA_DIR :
              case OP_BITA_EXT : break;

              case OP_EORA_IND : /*NZV*/
              case OP_EORA_DIR :
              case OP_EORA_EXT : break;
              case OP_ADCA_IND : /*HNZVC*/
              case OP_ADCA_DIR :
              case OP_ADCA_EXT : break;
              case OP_ORAA_IND : /*NZV*/
              case OP_ORAA_DIR :
              case OP_ORAA_EXT : break;
              case OP_ADDA_IND : /*HNZVC*/
              case OP_ADDA_DIR :
              case OP_ADDA_EXT : break;
              case OP_CPXY_IND : /*NZVC*/
              case OP_CPXY_DIR :
              case OP_CPXY_EXT : break;

              case OP_JSR_IND  :
              case OP_JSR_DIR  :
              case OP_JSR_EXT  :
                printf("JSR_EXT ea=%04X\n", core->operand);
                core->busdat  = core->regs.pc;
                core->regs.pc = core->operand;
                core->state = STATE_PUSH_L; // not H, push happens L first
                break;

              case OP_LDS_IND  : /*NZV*/
              case OP_LDS_DIR  :
              case OP_LDS_EXT  : break;
              case OP_STS_IND  : /*NZV*/
              case OP_STS_DIR  :
              case OP_STS_EXT  : break;


              case OP_SUBB_IND :/*NZVC*/
              case OP_SUBB_DIR :
              case OP_SUBB_EXT : break;
              case OP_CMPB_IND :/*NZVC*/
              case OP_CMPB_DIR :
              case OP_CMPB_EXT : break;
              case OP_SBCB_IND :/*NZVC*/
              case OP_SBCB_DIR :
              case OP_SBCB_EXT : break;
              case OP_ADDD_IND : /*NZVC*/
              case OP_ADDD_DIR :
              case OP_ADDD_EXT : break;
              case OP_ANDB_IND :/*NZV*/ 
              case OP_ANDB_DIR :
              case OP_ANDB_EXT : break;
              case OP_BITB_IND :/*NZV*/ 
              case OP_BITB_DIR :
              case OP_BITB_EXT : break;

              case OP_LDAA_IND :/*NZV*/
              case OP_LDAA_DIR :
              case OP_LDAA_EXT :
                core->regs.d = (core->regs.d & 0x00FF) | (core->busdat & 0xFF) << 8;
                tmp = core->regs.d >> 8;
                core->regs.flags.Z = tmp == 0;
                core->regs.flags.N = tmp >> 7;
                core->regs.flags.V = 0;
                printf("LDAA_DIR_EXT_IND %02X\n", core->busdat & 0xFF);
                break;

              case OP_LDAB_IND :/*NZV*/
              case OP_LDAB_DIR :
              case OP_LDAB_EXT :
                core->regs.d = (core->regs.d & 0xFF00) | (core->busdat & 0xFF);
                tmp = core->regs.d & 0xFF;
                core->regs.flags.Z = tmp == 0;
                core->regs.flags.N = tmp >> 7;
                core->regs.flags.V = 0;
                printf("LDAB_DIR_EXT_IND %02X\n", core->busdat & 0xFF);
                break;
              case OP_LDD_IND  :/*NZV*/
              case OP_LDD_DIR  :
              case OP_LDD_EXT  :
                core->regs.d = core->busdat;
                tmp = core->regs.d;
                core->regs.flags.Z = tmp == 0;
                core->regs.flags.N = tmp >> 15;
                core->regs.flags.V = 0;
                printf("LDD_DIR_EXT_IND @%04X -> %04X\n", core->operand, core->busdat);
                break;

              case OP_LDXY_IND :/*NZV*/ 
              case OP_LDXY_DIR :
              case OP_LDXY_EXT :
                core->regs.x = core->busdat;
                tmp = core->regs.x;
                core->regs.flags.Z = tmp == 0;
                core->regs.flags.N = tmp >> 15;
                core->regs.flags.V = 0;
                printf("LDX_DIR_EXT_IND @%04X -> %04X\n", core->operand, core->busdat);
                break;

              case OP_STAA_IND : /*NZV*/
              case OP_STAA_DIR :
              case OP_STAA_EXT :
                core->busadr = core->operand;
                tmp = core->regs.d >> 8;
                core->busdat = tmp;
                core->regs.flags.Z = tmp == 0;
                core->regs.flags.N = tmp >> 7;
                core->regs.flags.V = 0;
                core->state = STATE_WRITEOP_L;
                printf("STAA_DIR_EXT_IND\n");
                break;

              case OP_STAB_IND :/*NZV*/ 
              case OP_STAB_DIR :
              case OP_STAB_EXT :
                core->busadr = core->operand;
                tmp = core->regs.d & 0xFF;
                core->busdat = tmp;
                core->regs.flags.Z = tmp == 0;
                core->regs.flags.N = tmp >> 7;
                core->regs.flags.V = 0;
                core->state = STATE_WRITEOP_L;
                printf("STAB_DIR_EXT_IND\n");
                break;

              case OP_EORB_IND :/*NZV*/
              case OP_EORB_DIR :
              case OP_EORB_EXT : break;
              case OP_ADCB_IND : /*HNZVC*/
              case OP_ADCB_DIR :
              case OP_ADCB_EXT : break;
              case OP_ORAB_IND : /*NZV*/
              case OP_ORAB_DIR :
              case OP_ORAB_EXT : break;
              case OP_ADDB_IND : /*HNZVC*/
              case OP_ADDB_DIR :
              case OP_ADDB_EXT : break;

              case OP_STD_IND  :/*NZV*/
              case OP_STD_DIR  :
              case OP_STD_EXT  :
                core->busdat = core->regs.d;
                core->busadr = core->opcode;
                tmp = core->regs.d;
                core->regs.flags.Z = tmp == 0;
                core->regs.flags.N = tmp >> 7;
                core->regs.flags.V = 0;
                core->state = STATE_WRITEOP_H;
                printf("STD DIR_EXT_IND @%04X <- %04X\n", core->busadr, core->busdat);
                break;

              case OP_STXY_IND :/*NZV*/
              case OP_STXY_DIR :
              case OP_STXY_EXT :
                core->busdat = core->regs.x;
                core->busadr = core->opcode;
                tmp = core->regs.x;
                core->regs.flags.Z = tmp == 0;
                core->regs.flags.N = tmp >> 7;
                core->regs.flags.V = 0;
                core->state = STATE_WRITEOP_H;
                printf("STX DIR_EXT_IND @%04X <- %04X\n", core->busadr, core->busdat);
                break;

            } //normal opcodes
          break;

        case STATE_EXECUTE_18:
          printf("STATE_EXECUTE_18 op %02X operand %04X\n", core->opcode, core->operand);
          core->prefix = 0; //prepare for next opcode
          core->state = STATE_FETCHOPCODE; //default action when nothing needs writing
          switch(core->opcode)
            {
            }
            break;

        case STATE_EXECUTE_1A:
          printf("STATE_EXECUTE_1A op %02X operand %04X\n", core->opcode, core->operand);
          core->prefix = 0; //prepare for next opcode
          core->state = STATE_FETCHOPCODE; //default action when nothing needs writing
          switch(core->opcode)
            {
            }
            break;

        case STATE_EXECUTE_CD:
          printf("STATE_EXECUTE_CD op %02X operand %04X\n", core->opcode, core->operand);
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
          hc11_core_writeb(core,core->busadr, core->busdat & 0xFF);
          core->state = STATE_FETCHOPCODE;
          break;

        case STATE_PUSH_L:
          core->busadr = core->regs.sp;
          hc11_core_writeb(core, core->busadr, core->busdat & 0xFF);
          core->regs.sp = core->regs.sp - 1;
          core->state = STATE_PUSH_H;
          break;

        case STATE_PUSH_H:
          core->busadr = core->regs.sp;
          hc11_core_writeb(core, core->busadr, core->busdat >> 8);
          core->regs.sp = core->regs.sp - 1;
          core->state = STATE_FETCHOPCODE;
          break;

        case STATE_PULL_H:
          core->regs.sp = core->regs.sp + 1;
          core->busadr = core->regs.sp;
          core->busdat = hc11_core_readb(core, core->busadr) << 8;
          core->state = STATE_PULL_L;
          break;

        case STATE_PULL_L:
          core->regs.sp = core->regs.sp + 1;
          core->busadr = core->regs.sp;
          core->busdat = core->busdat | hc11_core_readb(core, core->busadr);
          switch(core->pulsel)
            {
              case PULL_CCR: core->regs.ccr = core->busdat & 0xFF; break;
              case PULL_B  : core->regs.d  = (core->regs.d & 0xFF00) | (core->busdat & 0xFF);      break;
              case PULL_A  : core->regs.d  = (core->regs.d & 0x00FF) | (core->busdat & 0xFF) << 8; break;
              case PULL_X  : core->regs.x  = core->busdat; break;
              case PULL_Y  : core->regs.y  = core->busdat; break;
              case PULL_PC : core->regs.pc = core->busdat; break;
            }
          core->state = STATE_FETCHOPCODE;
          break;
      }//switch
  }

