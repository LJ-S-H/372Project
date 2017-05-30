
#include "slc3.h"
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define DEBUG 0
#define INPUT_SIZE 50
#define SIZE_OF_MEM 1024
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
#define OPCODE_SHIFT_AMT 12
#define DEST_REG_SHIFT_AMT 9
#define SOURCE1_SHIFT_AMT 6

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

// you can define a simple memory module here for this program
Register memory[SIZE_OF_MEM]; // 32 words of memory enough to store simple program

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
            index = cpu->regFile[0];
            while (memory[index] != 0) {
              printf("%c", memory[index]);
              index++;
            }
            fflush(stdout);
            break;
    }
    return 0;
}



//Executes instructions on our simulated CPU.
int completeOneInstructionCycle(CPU_p cpu, ALU_p alu) {
    Register opcode, Rd, Rs1, Rs2, immed_offset, nzp, BEN, pcOffset; // fields for the IR
    int state = FETCH;
    while (state != DONE) {
        switch (state) {
            case FETCH: // microstates 18, 33, 35 in the book
                cpu->MAR = cpu->PC;
                cpu->PC++; // increment PC
                cpu->MDR = memory[cpu->MAR];
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
                        cpu->MDR = memory[memory[cpu->MAR]];
                        cpu->MAR = cpu->MDR;
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
                        cpu->MDR = memory[cpu->MAR];
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
                        memory[cpu->MAR] = cpu->MDR;
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

void printCurrentState(CPU_p cpu, ALU_p alu, int mem_Offset, unsigned short start_address) {
  int i , j;
  int numOfRegisters = sizeof(cpu->regFile)/sizeof(cpu->regFile[0]);
  printf("Registers            Instruction Cache               Memory\n");
  for (i = 0, j = mem_Offset; i < DISPLAY_SIZE; i++, j++) {
    if(i < numOfRegisters) {
      printf("R%d: x%04X     ", i, cpu->regFile[i] & 0xffff);  //don't use leading 4 bits
      if (i < 4) { //Instruction cache contents
          printf("x%04X: x%04X x%04X x%04X x%04X      ", start_address + (i * 4), memory[j], memory[j + 1], memory[j + 2], memory[j + 3]);
      } else if (i == 4) { //Data cache header
          printf("         Data L1 Cache              ");
      }                  
    } else if (i < 13) { //Print cache stuff
        printf("              ");
    } else if (i == 13) { //print PC, IR, etc...
        printf("PC:x%04X  IR:x%04X  A: x%04X  B: x%04X            ",cpu->PC + start_address, cpu->IR, alu->A  & 0xffff, alu->B & 0xffff);
    } else if (i == 14) {
        printf("MAR: x%04X MDR: x%04X CC: N:%d Z:%d P:%d             ",cpu->MAR, cpu->MDR & 0xffff, (cpu->CC & 4) > 0, (cpu->CC & 2) > 0, (cpu->CC & 1) > 0);
    } else {
        printf("                                                  ");
    }
    
    if (i < 13 && i > 4) { //Data cache contents
        printf("x%04X: x%04X x%04X x%04X x%04X      ", start_address + 0x0A00 + ((i - 5) * 4), memory[j], memory[j + 1], memory[j + 2], memory[j + 3]);
    }
    
    if(j < SIZE_OF_MEM){
      printf("x%04X: x%04X\n", j + start_address, memory[j]);
    } else {
      printf("\n");
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

void clearBreakpoints(Register breakpoints[]) {
    int i;
    for (i = 0; i < MAX_NUM_BKPTS; i++) {
        breakpoints[i] = DEFAULT_BKPT_VALUE;
    }
}

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

int getEmptyIndex(Register breakpoints[]) {
  int i;
    for (i = 0; i < MAX_NUM_BKPTS; i++) {
        if (breakpoints[i] == DEFAULT_BKPT_VALUE) {
            return i;
        }
    }
    return -1;
}

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
          //Initialize cpu fields;
          cpu_pointer->PC = 0;
          cpu_pointer->CC = Z;
        }
        break;
      case STEP:
        if (loadedProgram == 1) {
          int response = completeOneInstructionCycle(cpu_pointer, alu_pointer);
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
          int response = completeOneInstructionCycle(cpu_pointer, alu_pointer);
          int reachedBreakpoint = hitBreakpoint(breakpoints, cpu_pointer->PC, &numBreakpoints, 1);
          while (response != HALT && !reachedBreakpoint && cpu_pointer->PC != SIZE_OF_MEM) {
            response = completeOneInstructionCycle(cpu_pointer, alu_pointer);
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
        temp_offset = strtol(input, &temp, STRTOL_BASE) - start_address;
        if(temp_offset >= SIZE_OF_MEM || temp_offset < 0){
          printf("Not a valid address <ENTER> to continue.");
          getEnterInput();
        } else {
          offset = temp_offset;
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
