
#include "slc3.h"
#include <unistd.h>
#include <termios.h>

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
                opcode = cpu->IR & 0xF000;
                opcode = opcode >> 12;
                Rd = cpu->IR & 0x0E00;
                Rd = Rd >> 9;
                nzp = Rd;
                Rs1 = cpu->IR & 0x01C0;
                Rs1 = Rs1 >> 6;
                Rs2 = cpu->IR & 0x0007;
                BEN = cpu->CC & nzp; //current cc & instruction's nzp
                switch (opcode) {
                    case LEA: //pcOffset9
                    case LD:
                    case ST:
                    case BR:
                        pcOffset = 0x01FF & cpu->IR;
                        if (pcOffset & 0x0100) { //checks if pcOffset is negative
                            pcOffset = pcOffset | 0xFE00;
                        }
                        break;
                    case STR: //pcOffset6
                    case LDR:
                        pcOffset = 0x003F & cpu->IR; //0011 1111 & IR
                        if (pcOffset & 0x0020) {
                            pcOffset = pcOffset | 0xFFC0;
                        }
                        break;
                    case JSR: 
                        if (0x0800 & cpu->IR) { //if doing JSR, get pcOffset11
                            pcOffset = 0x07FF & cpu->IR; //0111 1111 1111 & IR
                            if (pcOffset & 0x0400) {
                                pcOffset = pcOffset | 0xF800;
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
                        cpu->MAR = cpu->IR & 0x00FF; //get the trapvector8
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
                        if ((cpu->IR & 0x20) == 0) {
                            alu->B = cpu->regFile[Rs2];
                        } else {
                            alu->B = cpu->IR & 0x1F; //get immed5.
                            if ((alu->B & 0x10) != 0) { //if first bit of immed5 = 1
                                alu->B = (alu->B | 0xFFE0);
                            }
                        }
                        break;
                    case NOT:
                        alu->A = cpu->regFile[Rs1];
                        break;
                    case LD:
                    case LDR:
                        cpu->MDR = memory[cpu->MAR];
                        break;
                    case ST:
                    case STR:
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
                        cpu->regFile[7] = cpu->PC; //R7 = PC
                        if (0x0800 & cpu->IR) { //if JSRR
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
                        cpu->regFile[Rd] = cpu->MDR;
                        setCC(cpu->regFile[Rd], cpu);
                        break;
                    case ST:
                    case STR:
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
  printf("        Registers           Memory\n");
  for (i = 0, j = mem_Offset; i < DISPLAY_SIZE; i++, j++) {
    if(i < numOfRegisters) {
      printf("        R%d: x%04X        ", i, cpu->regFile[i] & 0xffff);  //don't use leading 4 bits
    } else {
        switch(i){
          case 11:
            printf("   PC:x%04X   IR:x%04X   ",cpu->PC + start_address, cpu->IR);
            break;
          case 12:
            printf("   A: x%04X   B: x%04X   ",alu->A  & 0xffff, alu->B & 0xffff); //don't use leading 4 bits
            break;
          case 13:
            printf("  MAR:x%04X MDR: x%04X   ",cpu->MAR, cpu->MDR & 0xffff); //don't use leading 4 bits
            break;
          case 14:
            printf("      CC: N:%d Z:%d P:%d    ",(cpu->CC & 4) > 0, (cpu->CC & 2) > 0, (cpu->CC & 1) > 0);
            break;
          default:
            printf("                         ");
            break;
        }
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
void getEnterInput(char error) {
  while(1){
    scanf("%c",&error);
    scanf("%c",&error);
    if(error == '\n'){
      break;
    }
  }
}

int main(int argc, char * argv[]) {
    CPU_p cpu_pointer = malloc(sizeof(struct CPU_s));
    ALU_p alu_pointer = malloc(sizeof(struct ALU_s));
    cpu_pointer->PC = 0;
    cpu_pointer->CC = Z;
    char input[50];
    int choice;
    char error;
    char buf[5];
    char *temp;
    int temp_offset;
    int offset = 0;
    unsigned short start_address = DEFAULT_ADDRESS;
    int loadedProgram = 0;
    int programHalted = 0;
    cpu_pointer->regFile[0] = 0x1E; //R0 = 30
    cpu_pointer->regFile[7] = 0x5; //R7 = 5

  while (1){
    printf("Welcome to the LC-3 Simulator Simulator\n");
	  printCurrentState(cpu_pointer, alu_pointer, offset, start_address);
	  printf("Select: 1) Load, 3) Step, 4) Run, 5) Display Mem, 9) Exit\n> ");
    scanf("%d", &choice);
    switch(choice){
      case LOAD:
        printf("File name: ");
        scanf("%s", input);
        FILE *fp = fopen(input, "r");
        if(fp == NULL){
          printf("Error: File not found. Press <ENTER> to continue");
          getEnterInput(error);
        } else {
          int i = 0;
          while(!feof(fp)) {
            if(i == 0){
              fgets(buf, 5, fp);
              start_address = strtol(buf, &temp, 16);
              fgets(buf,3, fp);
            }
            fgets(buf, 5, fp);
            if(i >= SIZE_OF_MEM){
              printf("Error: Not enough memory");
              break;
            }
            memory[i] = strtol(buf, &temp, 16);
            i++;
            fgets(buf,3, fp);
          }
          fclose(fp);
          loadedProgram = 1;
          programHalted == 0;
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
            printf("\n======Program halted.======\n");
          }
        } else if (programHalted == 1){
          printf("Your program has halted. Please load another program. \nPress <ENTER> to continue");
          getEnterInput(error);
        } else {
          printf("Please load a program first. Press <ENTER> to continue");
          getEnterInput(error);
        }
        break;
      case RUN:
        if (loadedProgram == 1) {
          int response = completeOneInstructionCycle(cpu_pointer, alu_pointer);
          while (response != HALT) {
            response = completeOneInstructionCycle(cpu_pointer, alu_pointer);
          }
          loadedProgram = 0;
          programHalted = 1;
          printf("\n======Program halted.======\n");
        } else if (programHalted == 1){
          printf("Your program has halted. Please load another program. \nPress <ENTER> to continue");
          getEnterInput(error);
        } else {
          printf("Please load a program first. Press <ENTER> to continue");
          getEnterInput(error);
        }
        break;
      case DISPLAY_MEM:

        printf("Starting Address: ");
        scanf("%s", input);
        temp_offset = strtol(input, &temp, 16) - start_address;
        if(temp_offset >= SIZE_OF_MEM || temp_offset < 0){
          printf("Not a valid address <ENTER> to continue.");
          getEnterInput(error);
        } else {
          offset = temp_offset;
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
