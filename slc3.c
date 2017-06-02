/**
  * Authors: Lovejit Hari
  *          Vladimir Kaganyuk
  *          Dongsheng Han
  */

#include "slc3.h"
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

Register memory[SIZE_OF_MEM]; // 32 words of memory enough to store simple program
Cache_Entry instructionCache[SIZE_OF_CACHE];
Cache_Entry dataCache[SIZE_OF_CACHE];

//Sets the condition codes, given a result.
void setCC(short result, CPU_p cpu) {
    if (result < 0) { //Negative result
        cpu->CC = N;
    } else if (result == 0) { //Result = 0
        cpu->CC = Z;
    } else { //Positive result
        cpu->CC = P;
    }
}

//Prints out the register values, the IR, PC, MAR, and MDR.
void printCurrentState(CPU_p cpu, ALU_p alu, int mem_Offset, unsigned short start_address);
void getData(CPU_p cpu);

//C equivalent of LC3's GETC
char getch() {
    char buf = 0;
    struct termios old = {0};
    if (tcgetattr(0, &old) < 0)
        perror("tcsetattr()");
    
	old.c_lflag &= ~ICANON;
    old.c_lflag &= ~ECHO;
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;
    
	if (tcsetattr(0, TCSANOW, &old) < 0)
        perror("tcsetattr ICANON");
    
	if (read(0, &buf, 1) < 0)
            perror ("read()");
    
	old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;
    
	if (tcsetattr(0, TCSADRAIN, &old) < 0)
        perror ("tcsetattr ~ICANON");
    
	return (buf);
}

//Function to handle TRAP routines.
int trap(int trap_vector, CPU_p cpu) {
    int index;
    switch(trap_vector) {
        case HALT:
            return HALT;
        case GETC:
            cpu->regFile[0] = getch();
            break;
        case OUT:
            printf("%c", cpu->regFile[0]);
            fflush(stdout);
            break;
        case PUTS:
            cpu->MAR = cpu->regFile[0];
            getData(cpu);
            while (cpu->MDR != 0) {
              printf("%c", cpu->MDR);
              cpu->MAR++;
              getData(cpu);
            }
            fflush(stdout);
            break;
    }
    return 0;
}

//Sets all the cache values to zero.
void initializeCaches() {
    int i, j;
    for (i = 0; i < SIZE_OF_CACHE; i++) {
          instructionCache[i].entryInfo = 0;
          instructionCache[i].data = 0;
          dataCache[i].entryInfo = 0;
          dataCache[i].data = 0;
    }
}

//Accesses memory and updates the cache.
void accessMemory(CPU_p cpu, Register cacheIndex, Cache_Entry cache[]) {
    usleep(MICROSECONDS_TO_SLEEP); //Sleep to simulate memory accessing in the real world.
    cpu->MDR = memory[cpu->MAR]; //Load the data from memory.
    cache[cacheIndex].data = cpu->MDR; //Put the data into the dataCache.
}

//Places the current instruction into the MDR. Checks the instruction cache, and accesses
//memory if necessary.
void getInstruction(CPU_p cpu) {
    Register memAddress = cpu->MAR;
    int index = memAddress % SIZE_OF_CACHE;
    unsigned short tagFromAddress = memAddress / SIZE_OF_CACHE;
    
    if (!(instructionCache[index].entryInfo & VALID_BIT_MASK)) { //validBit not set
        instructionCache[index].entryInfo = instructionCache[index].entryInfo | (VALID_BIT_MASK + tagFromAddress);
        accessMemory(cpu, index, instructionCache);
    } else {
        unsigned short tagFromCache = instructionCache[index].entryInfo & TAG_MASK;
        if (tagFromCache == tagFromAddress) {
            cpu->MDR = instructionCache[index].data;
        } else {
            instructionCache[index].entryInfo &= CLEAR_TAG_MASK; //Clear tag
            instructionCache[index].entryInfo = instructionCache[index].entryInfo | tagFromAddress;
            accessMemory(cpu, index, instructionCache);
        }
    }
}

//Writes data from the dataCache to the main memory.
void writeToMemory(CPU_p cpu, Register writeAddress, Register cacheIndex) {
    usleep(MICROSECONDS_TO_SLEEP); //Sleep because accessing memory.
    memory[writeAddress] = dataCache[cacheIndex].data;
}

//Writes data to the cache and sets the appropriate bits. If a dirty bit is encountered, 
//it initiates the write back (to memory) process by calling writeToMemory.
void writeData(CPU_p cpu) {
    Register memAddress = cpu->MAR;
    Register index = memAddress % SIZE_OF_CACHE;
    Register tagFromAddress = memAddress / SIZE_OF_CACHE;
    Register tagFromCache = dataCache[index].entryInfo & TAG_MASK;
    
    if (dataCache[index].entryInfo & DIRTY_BIT_MASK) { //If dirty bit is set need to write to mem.
        writeToMemory(cpu, (tagFromCache * SIZE_OF_CACHE) + index, index);
    }
    
    dataCache[index].data = cpu->MDR;
    dataCache[index].entryInfo = dataCache[index].entryInfo | (DIRTY_AND_VALID_BIT_MASK + tagFromAddress); //Set valid bit, dirty bit, and tag.           
}

//Loads data into the MDR using the address in the MAR. Checks the data cache, and accesses memory if a
//read miss is encountered.
void getData(CPU_p cpu) {
    //entryInfo: (12 unused bits) + (1 bit valid bit) + (1 bit dirty bit) + (2 bits tag) 
    Register memAddress = cpu->MAR;
    int index = memAddress % SIZE_OF_CACHE;
    unsigned short tagFromAddress = memAddress / SIZE_OF_CACHE;
    unsigned short tagFromCache = dataCache[index].entryInfo & TAG_MASK;
    
    if (!(dataCache[index].entryInfo & VALID_BIT_MASK)) { //validBit not set, set valid bit and load data from mem into cache and the MDR.
        dataCache[index].entryInfo = dataCache[index].entryInfo | (VALID_BIT_MASK + tagFromAddress);
        accessMemory(cpu, index, dataCache);
    } else if (tagFromCache == tagFromAddress) { //Read hit, load the MDR from the data cache.
            cpu->MDR = dataCache[index].data;
    } else { //Read miss, set bits and read from mem. Write to mem if necessary (if encounter dirty bit).
        if (dataCache[index].entryInfo & DIRTY_BIT_MASK) { //If dirty bit set need to write to mem.
            writeToMemory(cpu, (tagFromCache * SIZE_OF_CACHE) + index, index);
        }
    
        dataCache[index].entryInfo &= CLEAR_TAG_AND_DIRTY_BIT_MASK; //Clear tag and dirty bit.
        dataCache[index].entryInfo = dataCache[index].entryInfo | tagFromAddress;
        accessMemory(cpu, index, dataCache);
    }
}

//Executes instructions on our simulated CPU.
int completeOneInstructionCycle(CPU_p cpu, ALU_p alu, unsigned short start_address) {
    Register opcode, Rd, Rs1, Rs2, immed_offset, nzp, BEN, pcOffset, j; // fields for the IR
    int state = FETCH;
    while (state != DONE) {
        switch (state) {
            case FETCH: // microstates 18, 33, 35 in the book
                cpu->MAR = cpu->PC;
                cpu->PC++; // increment PC
                getInstruction(cpu);
                //cpu->MDR = memory[cpu->MAR];
                cpu->IR = cpu->MDR;

                //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                #if DEBUG == 1
                printf("\n===========FETCH==============\n");
                printCurrentState(cpu,alu, 0, DEFAULT_ADDRESS);
                #endif
                //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                state = DECODE;
                break;
            case DECODE: // microstate 32
                // get the fields out of the IR
                opcode = cpu->IR & OPCODE_MASK;
                opcode = opcode >> OPCODE_SHIFT_AMT;
                Rd = cpu->IR & DEST_REG_MASK;
                Rd = Rd >> DEST_REG_SHIFT_AMT;
                nzp = Rd;
                Rs1 = cpu->IR & SOURCE1_REG_MASK;
                Rs1 = Rs1 >> SOURCE1_SHIFT_AMT;
                Rs2 = cpu->IR & SOURCE2_REG_MASK;
                BEN = cpu->CC & nzp; //current cc & instruction's nzp
                switch (opcode) {
                    case LEA: //pcOffset9
                    case LD:
                    case ST:
                    case BR:
		            case LDI:
                    case STI:
                        pcOffset = PCOFFSET9_MASK & cpu->IR;
                        if (pcOffset & BIT_8_MASK) { //checks if pcOffset is negative
                            pcOffset = pcOffset | INVERSE_PCOFFSET9_MASK;
                        }
                        break;
                    case STR: //pcOffset6
                    case LDR:
                        pcOffset = PCOFFSET6_MASK & cpu->IR; //0011 1111 & IR
                        if (pcOffset & BIT_5_MASK) {
                            pcOffset = pcOffset | INVERSE_PCOFFSET6_MASK;
                        }
                        break;
                    case JSR: 
                        if (BIT_11_MASK & cpu->IR) { //if doing JSR, get pcOffset11
                            pcOffset = PCOFFSET11_MASK & cpu->IR; //0111 1111 1111 & IR
                            if (pcOffset & BIT_10_MASK) {
                                pcOffset = pcOffset | INVERSE_PCOFFSET11_MASK;
                            }
                        }
                        break;
                    default: 
                        break;
                }

                //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                #if DEBUG == 1
                printf("\n===========DECODE==============\n");
                printCurrentState(cpu, alu, 0, DEFAULT_ADDRESS);
                #endif
                //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

                state = EVAL_ADDR;
                break;
            case EVAL_ADDR:
                switch (opcode) {
                    case TRAP:
                        cpu->MAR = cpu->IR & TRAP_VECTOR_8_MASK; //get the trapvector8
                        break;
                    case LD:
                    case ST:
                        cpu->MAR = cpu->PC + pcOffset;
                        break;
                    case STR:
                    case LDR:
                        cpu->MAR = cpu->regFile[Rs1] + pcOffset;
                    case BR:
                        if (BEN) {
                            cpu->PC = cpu->PC + pcOffset;
                        }
                        break;
                    case LDI:
                    case STI:
                        cpu->MAR = cpu->PC + pcOffset;
                        getData(cpu);
                        //cpu->MDR = memory[memory[cpu->MAR]];
                        cpu->MAR = cpu->MDR - start_address;
                        break;
                    default:
                        break;
                }

                //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                #if DEBUG == 1
                printf("\n===========EVAL_ADDR==============\n");
                printCurrentState(cpu, alu, 0, DEFAULT_ADDRESS);
                #endif
                //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

                state = FETCH_OP;
                break;
            case FETCH_OP:
                switch (opcode) {
                    case ADD:
                    case AND:
                        alu->A = cpu->regFile[Rs1];
                        if ((cpu->IR & BIT_5_MASK) == 0) {
                            alu->B = cpu->regFile[Rs2];
                        } else {
                            alu->B = cpu->IR & IMMED5_MASK; //get immed5.
                            if ((alu->B & BIT_4_MASK) != 0) { //if first bit of immed5 = 1
                                alu->B = (alu->B | INVERSE_IMMED5_MASK);
                            }
                        }
                        break;
                    case NOT:
                        alu->A = cpu->regFile[Rs1];
                        break;
                    case LD:
                    case LDR:
                    case LDI:
                        //cpu->MDR = memory[cpu->MAR];
                        getData(cpu);
                        break;
                    case ST:
                    case STR:
                    case STI:
                        cpu->MDR = cpu->regFile[Rd];
                        break;
                    default:
                        break;
                    }
                //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                #if DEBUG == 1
                printf("\n===========FETCH_OP==============\n");
                printCurrentState(cpu, alu, 0, DEFAULT_ADDRESS);
                #endif
                //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

                state = EXECUTE;
                break;
            case EXECUTE:
                switch (opcode) {
                    case ADD:
                        alu->R = alu->A + alu->B;
                        setCC(alu->R, cpu);
                        break;
                    case AND:
                        alu->R = alu->A & alu->B;
                        setCC(alu->R, cpu);
                        break;
                    case NOT:
                        alu->R = ~(alu->A);
                        setCC(alu->R, cpu);
                        break;
                    case TRAP:
                        if (trap(cpu->MAR, cpu) == HALT) //checks if program should halt
                            return HALT;
                        break;
                    case JMP:
                        cpu->PC = cpu->regFile[Rs1];
                        break;
                    case JSR:
                        cpu->R7 = cpu->PC; //R7 = PC
                        if (BIT_11_MASK & cpu->IR) { //if JSRR
                            cpu->PC += pcOffset; //PC = PC + PCoffset11
                        } else { //else doing JSR
                            cpu->PC = cpu->regFile[Rs1]; //PC = BaseReg
                        }
                        break;
                    case PUP:
                        if(cpu->IR & POP_MASK) { //Doing pop
                            cpu->regFile[Rd] = memory[cpu->R6 - start_address];
                            cpu->R6++;
                        } else { //Doing push
                            cpu->R6--;
                            memory[cpu->R6 - start_address] = cpu->regFile[Rd];
                        }                    
                        break;
                    default:
                        break;
                }
                //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                #if DEBUG == 1
                printf("\n===========EXECUTE==============\n");
                printCurrentState(cpu, alu, 0, DEFAULT_ADDRESS);
                #endif
                //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

                state = STORE;
                break;
            case STORE:
                switch (opcode) {
                    case ADD:
                        cpu->regFile[Rd] = alu->R;
                        break;
                    case AND:
                        cpu->regFile[Rd] = alu->R;
                        break;
                    case NOT:
                        cpu->regFile[Rd] = alu->R;
                        break;
                    case LD:
                    case LDR:
                    case LDI:
                        cpu->regFile[Rd] = cpu->MDR;
                        setCC(cpu->regFile[Rd], cpu);
                        break;
                    case ST:
                    case STR:
                    case STI:
                        writeData(cpu);                    
                        //memory[cpu->MAR] = cpu->MDR;
                        break;
                    case LEA:
                        cpu->regFile[Rd] = cpu->PC + pcOffset;
                        setCC(cpu->regFile[Rd], cpu);
                        break;
                    default:
                        break;
                }

                state = DONE;
                break;
        }
    }
    return 0;
}

//Prints the debug monitor (registers, both caches, and some of the memory)
void printCurrentState(CPU_p cpu, ALU_p alu, int mem_Offset, unsigned short start_address) {
  int i , j, temp;
  int numOfRegisters = sizeof(cpu->regFile)/sizeof(cpu->regFile[0]);
  printf("Registers            Instruction Cache               Memory\n");
  for (i = 0, j = mem_Offset; i < DISPLAY_SIZE; i++, j++) {
    temp = i * NUM_INST_CACHE_LINES;
    if(i < numOfRegisters) {
      printf("R%d: x%04X     ", i, cpu->regFile[i] & NEG_NUM_MASK);  //don't use leading 4 bits
      if (i < NUM_INST_CACHE_LINES) { //Instruction cache contents
          printf("x%04X: x%04X x%04X x%04X x%04X      ", start_address + temp, instructionCache[temp].data, instructionCache[temp + 1].data, instructionCache[temp + 2].data, instructionCache[temp + 3].data);
      } else if (i == NUM_INST_CACHE_LINES) { //Data cache header
          printf("         Data L1 Cache              ");
      }                  
    } else if (i < NUM_DATA_CACHE_LINES) { //Print cache stuff
        printf("              ");
    } else if (i == NUM_DATA_CACHE_LINES) { //print PC, IR, etc...
        printf("PC:x%04X  IR:x%04X  A: x%04X  B: x%04X            ",cpu->PC + start_address, cpu->IR, alu->A  & NEG_NUM_MASK, alu->B & NEG_NUM_MASK);
    } else if (i == NUM_DATA_CACHE_LINES + 1) {
        printf("MAR: x%04X MDR: x%04X CC: N:%d Z:%d P:%d             ",cpu->MAR + start_address, cpu->MDR & NEG_NUM_MASK, (cpu->CC & NEG_BIT_MASK) > 0, (cpu->CC & ZERO_BIT_MASK) > 0, (cpu->CC & 1) > 0);
    } else {
        printf("                                                  ");
    }
    
    if (i < NUM_DATA_CACHE_LINES && i > NUM_INST_CACHE_LINES) { //Data cache contents
        printf("x%04X: x%04X x%04X x%04X x%04X      ", start_address + DATA_CACHE_OFFSET + ((i - (NUM_INST_CACHE_LINES + 1)) * NUM_INST_CACHE_LINES), dataCache[temp].data, dataCache[temp + 1].data, dataCache[temp + 2].data, dataCache[temp + 3].data);
    }
    
    if(j < SIZE_OF_MEM && j >= 0){
      printf("x%04X: x%04X", j + start_address, memory[j]);
      Register index = j % SIZE_OF_CACHE;
    
      if (dataCache[index].entryInfo & DIRTY_BIT_MASK) { //If dirty bit set need to write to mem.
          printf("  *D*");
      }
      printf("\n");
    } else {
      printf("x%04X: x0000\n", j + start_address);
    }
  }
}

//Handles user input when an error message tells them
//to "Press <ENTER> to continue"
void getEnterInput() {
  char error;

  while(1){
    scanf("%c",&error);
    scanf("%c",&error);
    if(error == '\n'){
      break;
    }
  }
}

//Clears the breakpoints by setting them to the default value.
void clearBreakpoints(Register breakpoints[]) {
    int i;
    for (i = 0; i < MAX_NUM_BKPTS; i++) {
        breakpoints[i] = DEFAULT_BKPT_VALUE;
    }
}

//Checks to see if we've hit a breakpoint.
int hitBreakpoint(Register breakpoints[], Register PC, int *numBreakpoints, int remove) {
   int i;
    for (i = 0; i < MAX_NUM_BKPTS; i++) {
        if (breakpoints[i] == PC) {
            if (remove) {
              breakpoints[i] = DEFAULT_BKPT_VALUE;
              (*numBreakpoints)--;
            }                                                
            return 1;
        }
    }
    return 0;
}

//Returns the index of the first location in the breakpoints array that is empty.
int getEmptyIndex(Register breakpoints[]) {
  int i;
    for (i = 0; i < MAX_NUM_BKPTS; i++) {
        if (breakpoints[i] == DEFAULT_BKPT_VALUE) {
            return i;
        }
    }
    return -1;
}

//Prints all of the breakpoints that the user currently has set.
void printCurrentBreakpoints(Register breakpoints[], int start_address) {
    printf("\n======= Current Breakpoints =======\n");
    int i;
    for (i = 0; i < MAX_NUM_BKPTS; i++) {
        if (breakpoints[i] != DEFAULT_BKPT_VALUE) {
            printf("x%04X\n", breakpoints[i] + start_address);
        }
    }
    
    printf("===================================\n\n");
}

int main(int argc, char * argv[]) {
    CPU_p cpu_pointer = malloc(sizeof(struct CPU_s));
    ALU_p alu_pointer = malloc(sizeof(struct ALU_s));
    cpu_pointer->PC = 0;
    cpu_pointer->CC = Z;
    char input[INPUT_SIZE];
    char file_name[INPUT_SIZE];
    int choice;
    char buf[5];
    char *temp;
    int temp_offset;
    int offset = 0;
    int save_temp = 0;
    unsigned short start_address = DEFAULT_ADDRESS;
    int loadedProgram = 0;
    int programHalted = 0;
    int n;
    int owCheck;
    int saveCheck = 1;
    unsigned int start, end;
    Register breakpoints[MAX_NUM_BKPTS];
    int numBreakpoints = 0;
    clearBreakpoints(breakpoints);
    initializeCaches();

  while (1) {
    printf("           Welcome to the LC-3 Simulator Simulator\n");
	  printCurrentState(cpu_pointer, alu_pointer, offset, start_address);
	  printf("Select: 1) Load, 2) Save, 3) Step, 4) Run, 5) Display Mem, 6) Edit, 7) Set Bkpt, 8) Unset Bkpt, 9) Exit\n> ");
    scanf("%d", &choice);
    switch(choice){
      case LOAD:
        printf("File name: ");
        scanf("%s", input);
        FILE *fp = fopen(input, "r");
        if(fp == NULL){
          printf("Error: File not found. Press <ENTER> to continue");
          getEnterInput();
        } else {
          int i = 0;
          while(!feof(fp)) {
            if(i == 0){
              fgets(buf, 5, fp);
              start_address = strtol(buf, &temp, STRTOL_BASE);
              fgets(buf,3, fp);
            }
            fgets(buf, 5, fp);
            if(i >= SIZE_OF_MEM){
              printf("Error: Not enough memory");
              break;
            }
            memory[i] = strtol(buf, &temp, STRTOL_BASE);
            i++;
            fgets(buf,3, fp);
          }
          fclose(fp);
          loadedProgram = 1;
          programHalted = 0;
          numBreakpoints = 0;
          clearBreakpoints(breakpoints);
          initializeCaches();
          //Initialize cpu fields;
          cpu_pointer->PC = 0;
          cpu_pointer->CC = Z;
        }
        break;
      case STEP:
        if (loadedProgram == 1) {
          int response = completeOneInstructionCycle(cpu_pointer, alu_pointer, start_address);
          if (response == HALT) {
            loadedProgram = 0;
            programHalted = 1;                        
            printf("\n======Program halted.======\nPress <ENTER> to continue.");
            numBreakpoints = 0;
            clearBreakpoints(breakpoints);
            getEnterInput();
          }
        } else if (programHalted == 1){
          printf("Your program has halted. Please load another program. \nPress <ENTER> to continue.");
          getEnterInput();
        } else {
          printf("Please load a program first. Press <ENTER> to continue");
          getEnterInput();
        }
        break;
      case RUN:
        if (loadedProgram == 1) {
          int response = completeOneInstructionCycle(cpu_pointer, alu_pointer, start_address);
          int reachedBreakpoint = hitBreakpoint(breakpoints, cpu_pointer->PC, &numBreakpoints, 1);
          while (response != HALT && !reachedBreakpoint && cpu_pointer->PC != SIZE_OF_MEM) {
            response = completeOneInstructionCycle(cpu_pointer, alu_pointer, start_address);
            reachedBreakpoint = hitBreakpoint(breakpoints, cpu_pointer->PC, &numBreakpoints, 1);
          }
          
          if (reachedBreakpoint) {
              printf("Reached breakpoint: x%04X\nPress <ENTER> to return to the menu.", cpu_pointer->PC + start_address);
              getEnterInput();
          } else {
            loadedProgram = 0;
            programHalted = 1;
            
            if (cpu_pointer->PC == SIZE_OF_MEM)
              printf("\n======= END OF MEMORY REACHED =======\nPlease include a HALT in your program to prevent this from happening.\nPress <ENTER> to continue.");
            else
              printf("\n======Program halted.======\nPress <ENTER> to continue.");
            numBreakpoints = 0;
            clearBreakpoints(breakpoints);
            getEnterInput();
          }
        } else if (programHalted == 1){
          printf("Your program has halted. Please load another program.\nPress <ENTER> to continue.");
          getEnterInput();
        } else {
          printf("Please load a program first. Press <ENTER> to continue.");
          getEnterInput();
        }
        break;
      case DISPLAY_MEM:
        printf("Starting Address: ");
        scanf("%s", input);
        temp_offset = strtol(input, &temp, STRTOL_BASE);
        if(temp_offset < 0 || temp_offset >= NEG_NUM_MASK){
          printf("Not a valid address <ENTER> to continue.");
          getEnterInput();
        } else {
          offset = temp_offset - start_address;
        }
        break;
	  case EDIT:
		  printf("The memory address to be edited: ");
		  scanf("%s", input);
		  temp_offset = strtol(input, &temp, STRTOL_BASE) - start_address;
		  if (temp_offset >= SIZE_OF_MEM || temp_offset < 0) {
			  printf("Not a valid address <ENTER> to continue.");
			  getEnterInput();
		  }
		  else {
			  printf("x%04X: x%04X\n", temp_offset + start_address, memory[temp_offset]);
			  printf("The new contents to be entered in hex: ");
			  scanf("%s", input);
			  memory[temp_offset] = strtol(input, &temp, STRTOL_BASE);
		  }
		  break;
      case SAVE:
        printf("\nEnter file name to save to: ");
        scanf("%s", file_name);
        FILE *fp1 = fopen(file_name, "r");
        if(fp1 == NULL) {
	      saveCheck = 1;
          n = 0;
        } else {
            n = 1;
            fclose(fp1);
        }
        
        if(n == 1) {
            printf("\nFile already exists, overwrite? 1 for yes, 2 for no: ");
            scanf("%d", &owCheck);   
            if(owCheck != 1) {
                saveCheck = 0;            
            }
        }
        
        if(saveCheck) {
            int i;
            FILE *fp2 = fopen(file_name, "w");
            printf("\nEnter start and end address like this: XXXX, XXXX:\n> ");
            scanf("%04X, %04X", &start, &end);
            if(start > end || start < 0 || end < 0 || end > SIZE_OF_MEM) {
                printf("Invalid address range");
                getEnterInput();
            } else {
                for(i = start; i <= end; i++) {
                    fprintf(fp2, "%04X\n", memory[i - start_address]);
                }
            }
            fclose(fp2);
        } 
        break;
      case SET_BKPT:
        if (loadedProgram == 0) {
            printf("Please load a program and try again. Press <ENTER> to continue.");
            getEnterInput();
            break;
        }
        
        if (numBreakpoints > 0)
          printCurrentBreakpoints(breakpoints, start_address); 
        
        if (numBreakpoints == MAX_NUM_BKPTS) {
          printf("You've already set %d breakpoints (the maximum amount).\nPlease unset one and try again. Press <ENTER> to continue.", MAX_NUM_BKPTS);
          getEnterInput();
          break;
        }
        printf("The memory address to break at: ");
		    scanf("%s", input);
		    temp_offset = strtol(input, &temp, STRTOL_BASE) - start_address;
		    if (temp_offset >= SIZE_OF_MEM || temp_offset < 0) {
			    printf("Not a valid address, press <ENTER> to continue.");
			    getEnterInput();
		    } else {
          if (hitBreakpoint(breakpoints, temp_offset, NULL, 0)) { //If there's already a breakpoint at this mem address.
            printf("It appears that you've already set a breakpoint at address: x%04X\nPress <ENTER> to continue.", temp_offset + start_address);
          } else {
            breakpoints[getEmptyIndex(breakpoints)] = temp_offset;
            numBreakpoints++;
            printf("Successfully set breakpoint at: x%04X\nPress <ENTER> to continue.", temp_offset + start_address); 
          }                   
          getEnterInput(); 
        }
          break;
      case UNSET_BKPT:
        if (numBreakpoints == 0) {
          printf("You haven't set any breakpoints yet. Please set one and try again.\nPress <ENTER> to continue.");
          getEnterInput();
          break;
        }
        
        printCurrentBreakpoints(breakpoints, start_address);
        
        printf("The memory address to unset: ");
		    scanf("%s", input);
		    temp_offset = strtol(input, &temp, STRTOL_BASE) - start_address;
		    if (temp_offset >= SIZE_OF_MEM || temp_offset < 0) {
			    printf("Not a valid address, press <ENTER> to continue.");
			    getEnterInput();
		    } else {
          int i;
          int temp = numBreakpoints;
          for (i = 0; i < MAX_NUM_BKPTS; i++) {
            if (breakpoints[i] == temp_offset) {
              breakpoints[i] = DEFAULT_BKPT_VALUE;
              numBreakpoints--;
              break;
            }
          }
          if (temp == numBreakpoints) { //Breakpoint not found in array.
            printf("Breakpoint to unset not found, press <ENTER> to continue.");
            getEnterInput();
          } else {
            printf("Successfully removed breakpoint at: x%04X\nPress <ENTER> to continue.", temp_offset + start_address); 
            getEnterInput(); 
          }
        }
          break;
      case EXIT:
        printf("Goodbye\n");
        return 0;
        break;
      default:
        printf("Error: Invalid selection\n");
        break;

    }

  }
  return 0;
}
