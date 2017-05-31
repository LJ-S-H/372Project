
#ifndef LC3_H
#define LC3_H

typedef unsigned short Register;

typedef struct ALU_s {
    Register A;
    Register B;
    Register R;
}
ALU_s;

typedef ALU_s * ALU_p;

typedef struct CPU_s {
    int regFile[8];
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
