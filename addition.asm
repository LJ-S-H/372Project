.ORIG x3000

LD R6 STACK_POINTER		;Loads R6 with x4000, the starting address of our stack.
LD R2 MAX_ASCII			;Loads R2 with the ASCII value of 0.
LD R3 ASCII_TRANSLATE	;Loads R3 with the ASCII value of 9.

GETNUM LEA R0 PROMPT	;Print enter number prompt
PUTS
GETC
OUT						;Prints the user's character input back out.
ADD R4, R0, #-10		;10 == 'Enter' Ascii value
BRz DONE_WITH_INPUT 	;If user pressed Enter, stop getting input.
ADD R1, R0, R2
BRp PRINTERROR			;The character is greater than the ASCII value of 9.
ADD R0, R0, R3
BRn PRINTERROR			;The character is greater than the ASCII value of 0.
PUSH_NUM ADD R6, R6, #-1 ;Push the number onto the stack.
STR R0, R6, #0			 ;Instruction = xD000, Line 15 in addition.hex
LD R0 ENTER_ASCII_VALUE
OUT						;Prints a newline, so formatting is not messed up.
LD R0 STACK_TOP
NOT R0, R0
ADD R0, R0, #1
ADD R0, R6, R0			;Test to see if stack is full (reached the top of the stack)
BRz REACHED_TOP
BRnzp GETNUM			;Get another number.

REACHED_TOP LEA R0, TOP_OUTPUT
PUTS					;Print a message letting the user know they've filled the stack.
DONE_WITH_INPUT LEA R0 ADDITION
JSRR R0					;Call the ADDITION subroutine.
HALT

PRINTERROR LEA R0 ERRORPROMPT
PUTS					;Prints the invalid input error message.
BRnzp GETNUM

ADDITION AND R1, R1, #0
STI R1, SUM_LOCATION	;Sets the sum location to zero.
LD R2, STACK_POINTER
NOT R2, R2
ADD R2, R2, #1			;Sets R2 = -x4000, the bottom of the stack negated.

ADD_ITEM LDR R0, R6, #0 ;Pops the number off the stack, and into R0.
ADD R6, R6, #1 			;Instruction = xD020, Line 38 in addition.hex
ADD R1, R0, R1
STI R1, SUM_LOCATION	;Stores the temporary sum into memory.

ADD R0, R6, R2			;Tests to see if R6 has reached the bottom of the stack (aka the stack is empty)
BRn ADD_ITEM			;If the stack isn't empty, go process the next number.

LDI R0, SUM_LOCATION	;Load the final sum from memory into R0.
RET

ENTER_ASCII_VALUE .FILL #10
STACK_TOP .FILL x3FFA
STACK_POINTER .FILL x3FFF
SUM_LOCATION .FILL x3500
ASCII_TRANSLATE .FILL #-48
MAX_ASCII .FILL #-57
PROMPT .STRINGZ "Please enter a number between 0 and 9: "
ERRORPROMPT .STRINGZ "\nInvalid number.\n"
OUTPUT_PROMPT .STRINGZ "\n\nYour sum is now in R0.\nPress any key to continue...\n"
TOP_OUTPUT .STRINGZ "\nYou've reached the top of the stack, your sum will now be computed.\n"

.END