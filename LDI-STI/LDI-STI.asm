		.ORIG 	x3000
		AND	R0,R0,#0
		ADD	R0,R0,#1
		STI 	R0, A
		LDI 	R1, A
A 		.FILL 	x0009
		HALT
		.END
			
