
#ifndef LC3_H
#define LC3_H
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define DEBUG 0
#define SIZE_OF_MEM 2048
#define DISPLAY_SIZE 16
#define DEFAULT_ADDRESS 0x3000

#define FETCH 0
#define DECODE 1
#define EVAL_ADDR 2
#define FETCH_OP 3
#define EXECUTE 4
#define STORE 5
#define DONE 6

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

#define N 4 //100
#define Z 2 //010
#define P 1 //001

#define LOAD 1
#define STEP 3
#define RUN 4
#define DISPLAY_MEM 5
#define EXIT 9

#define GETC 32 //0x20
#define OUT 33 //0x21
#define PUTS 34 //0x22
#define HALT 37 //0x25

typedef unsigned short Register;

typedef struct ALU_s {
    short A;
    short B;
    short R;
}
ALU_s;

typedef ALU_s * ALU_p;

typedef struct CPU_s {
    int regFile[8];
    int n, z, p;
    Register IR;
    Register PC;
    Register MAR;
    short MDR;
    Register CC;
}
CPU_s;

typedef struct CPU_s * CPU_p;

#endif
