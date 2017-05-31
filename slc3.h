/**
  * Authors: Lovejit Hari
  *          Vladimir Kaganyuk
  *          Dongsheng Han
  */
#ifndef LC3_H
#define LC3_H

#define DEBUG 0
#define INPUT_SIZE 50
#define SIZE_OF_MEM 0xFFFF
#define SIZE_OF_CACHE 0X4000
#define NUM_WORDS_IN_BLOCK 4
#define DISPLAY_SIZE 16
#define DEFAULT_ADDRESS 0x3000
#define STRTOL_BASE 16
#define MAX_NUM_BKPTS 4
#define DEFAULT_BKPT_VALUE 9999
#define CACHE_LINES 256
#define CACHE_BLOCK 4
#define R7 regFile[7]
#define OPCODE_MASK 0xF000
#define DEST_REG_MASK 0x0E00
#define SOURCE1_REG_MASK 0x01C0
#define SOURCE2_REG_MASK 0x0007
#define TRAP_VECTOR_8_MASK 0x00FF
#define IMMED5_MASK 0x1F
#define PCOFFSET6_MASK 0x003F
#define PCOFFSET9_MASK 0x01FF
#define PCOFFSET11_MASK 0x07FF
#define INVERSE_IMMED5_MASK 0xFFE0
#define INVERSE_PCOFFSET6_MASK 0xFFC0
#define INVERSE_PCOFFSET9_MASK 0xFE00
#define INVERSE_PCOFFSET11_MASK 0xF800
#define BIT_4_MASK 0x10
#define BIT_5_MASK 0x20
#define BIT_8_MASK 0x0100
#define BIT_10_MASK 0x0400
#define BIT_11_MASK 0x0800
#define CLEAR_TAG_MASK 0xFFFC
#define CLEAR_TAG_AND_DIRTY_BIT_MASK 0xFFF8
#define VALID_BIT_MASK 0x0008
#define DIRTY_BIT_MASK 0x0004
#define DIRTY_AND_VALID_BIT_MASK 0x000C
#define TAG_MASK 0x0003
#define OPCODE_SHIFT_AMT 12
#define DEST_REG_SHIFT_AMT 9
#define SOURCE1_SHIFT_AMT 6
#define MICROSECONDS_TO_SLEEP 2500
#define PUP_MASK 0x0020

#define FETCH 0
#define DECODE 1
#define EVAL_ADDR 2
#define FETCH_OP 3
#define EXECUTE 4
#define STORE 5
#define DONE 6
#define SET_BKPT 7
#define UNSET_BKPT 8

#define ADD 1
#define AND 5
#define NOT 9
#define TRAP 15
#define LD 2
#define ST 3
#define JMP 12
#define BR 0
#define JSR 4
#define LEA 14
#define RET 12
#define STR 7
#define LDR 6
#define LDI 10
#define STI 11
#define PUP 13

#define N 4 //100
#define Z 2 //010
#define P 1 //001

#define LOAD 1
#define SAVE 2
#define STEP 3
#define RUN 4
#define DISPLAY_MEM 5
#define EDIT 6
#define BRKPT 7
#define EXIT 9

#define GETC 32 //0x20
#define OUT 33 //0x21
#define PUTS 34 //0x22
#define HALT 37 //0x25

typedef unsigned short Register;

typedef struct ALU_s {
    Register A;
    Register B;
    Register R;
}
ALU_s;

typedef ALU_s * ALU_p;

typedef struct CPU_s {
	Register regFile[8];
    int n, z, p;
    Register IR;
    Register PC;
    Register MAR;
    Register MDR;
    Register CC;
}
CPU_s;

typedef struct CPU_s * CPU_p;

typedef struct Cache_Entry {
    Register entryInfo;
    Register data;
}
Cache_Entry; 

#endif
