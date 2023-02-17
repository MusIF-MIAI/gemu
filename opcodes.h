#ifndef OPCODES_H
#define OPCODES_H

/* Operation codes and instruction formats
 * --------------------------------------- */

/* P format instructions */

#define ENS_OPCODE   0x02
#define ENS_2NDCHAR  0x10

#define INS_OPCODE   0x02
#define INS_2NDCHAR  0x20

#define LOFF_OPCODE  0x02
#define LOFF_2NDCHAR 0x40

#define LON_OPCODE   0x02
#define LON_2NDCHAR  0x80

#define LOLL_OPCODE  0x02
#define LOLL_2NDCHAR 0x91

#define NOP2_OPCODE  0x07
#define HLT_OPCODE   0x0A

/* PM format instructions */

#define JIE_OPCODE  0x53
#define JIE_2NDCHAR 0x20

#define JS2_OPCODE  0x53
#define JS2_2NDCHAR 0x40

#define JS1_OPCODE  0x53
#define JS1_2NDCHAR 0x80

#define JRT_OPCODE  0x41
#define JC_OPCODE   0x43
#define LA_OPCODE   0x68
#define TM_OPCODE   0x91
#define MVI_OPCODE  0x92
#define NI_OPCODE   0x94
#define CMI_OPCODE  0x95
#define CI_OPCODE   0x96
#define XI_OPCODE   0x97
#define PERI_OPCODE 0x9c
#define LPSR_OPCODE 0x9d
#define PER_OPCODE  0x9e
#define STR_OPCODE  0x84
#define LR_OPCODE   0xbc
#define CMR_OPCODE  0xbd
#define AMR_OPCODE  0xbe
#define SMR_OPCODE  0xbf

/* PMM Format Instructions */

#define MVC_OPCODE  0xd2
#define NC_OPCODE   0xd4
#define CMC_OPCODE  0xd5
#define OC_OPCODE   0xd6
#define XC_OPCODE   0xd7
#define UPK_OPCODE  0xd8
#define SR_OPCODE   0xd9
#define PK_OPCODE   0xdA
#define SL_OPCODE   0xdb
#define TL_OPCODE   0xdc
#define EDT_OPCODE  0xde
#define MVP_OPCODE  0xe8
#define CMP_OPCODE  0xe9
#define AP_OPCODE   0xea
#define SP_OPCODE   0xeb
#define MP_OPCODE   0xec
#define DP_OPCODE   0xed
#define PKS_OPCODE  0xee
#define UPKS_OPCODE 0xef
#define MVQ_OPCODE  0xf8
#define CMQ_OPCODE  0xf9
#define AD_OPCODE   0xfa
#define SD_OPCODE   0xfb
#define AB_OPCODE   0xfe
#define SB_OPCODE   0xff

/* from cpu fo. 10, 11 */

#endif
