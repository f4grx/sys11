/* Mini forth interpreter based on eForth with simplifications

   Overview
   ========
   This is a forth interpreter inspired by eForth, with simplifications.
   Its goal is to be as compact as possible so it's possible to assemble itself from Forth.
   The following simplifications have be made with respect to the usual eForth implementations:
   Memory map simplifications: just a 1K param stack, and a RAM zone with dic and data
     growing from the low addresses and the return stack growing from the high addresses
   No USER pointer, only one user is supported
   Only a single word list, no vocabularies for each user.
   Favor F2012 words and semantics over the older standards if different.
   
   Builtin words are stored in RODATA. They cant be changed, but can be
   overriden by user entries.
   
   Hardware Registers
   ------------------

   The forth interpreter uses registers for its internal use.
   SP is used as parameter stack pointer.
   Y  is used as return stack pointer.
   X and D can be used for calculations.

   Data structures
   ===============

   Interpreter variables
   ---------------------
   
   The state of the interpreter is described in several variables, stored in the internal HC11 RAM.
   Here is a description of what each variable does.
   IP   - The instruction pointer. Points to the next instruction to execute.
   HEREP - pointer to the next free byte in the Data Space
   LASTP - pointer to the last definition. Initialized to the last builtin word definition.
   BEHAP - pointer containing the word routine that will manage the parsed tokens. Either INTERP or COMPILE.
           This word routine is called with a word pointer on the parameter stack.
           BEHAP is changed to EXECUTE when executing the immediate word [
           BEHAP is changed to COMPILE when executing the immediate word ]
           COMPILE appends the word pointer on the parameter stack to the current definition
           EXECUTE executes the word by "calling" it.
   BASE - Contains the value of the current number conversion base (unused now)
   
   Dictionary and Data space
   -------------------------

   The data space is used to store new word definitions and user data structures.
   It grows from low addresses (0100h) to upper addresses in the external memory.

   User definitions are allocated in the data space. The definitions are added
   one after the other, linking the new one to the previous one. The pointer to
   the last definition is maintained in LAST. This pointer is initialized to the
   last entry in the builtin dictionary.

   Dictionary entry
   ----------------

   Each entry has the following structure:
   2 bytes: pointer to previous entry
   N bytes: the word itself, NULL terminated. Only ASCII values 32-127 are allowed.
   1 byte:  flags. It seems that we need 2 flags:
            - immediate: word has to be executed even while compiling.
            - compile only: word cannot be executed (control structures?).
   (NOTE: to save one byte per word, the flags can be set in the terminator itself
   if the name is terminated by a byte having the most significant bit set instead of zero.)
   The 3 previous bytes form the word header. It is only inspected when searching for words.
   2 bytes: code pointer (ITC) - address of native code that implements this routine. Special value ENTER
            is used to handle compiled forth definitions.
   A forth word pointer, as used in user definitions, is a pointer to the code pointer for the word.
   
   Parameter stack
   ---------------
   The parameter stack is used to store temporary data items. It shoud not grow
   to an extremely large value, so it size is fixed when the interpreter is
   initialized.
   The stack grows down, it starts at the end of the RAM, and the storage
   PUSH is post-decrement, push LSB first.
   POP  is pre-increment, pull MSB first.
   The stack pointer always point at the next free location in the stack.
   For the moment underflow and overflows are not detected.

   Return stack
   ------------
   The return stack is used to push return addresses when nesting word
   executions. It is also used to manage control flow structures. Since this
   space is used a lot by complex programs, it is expected that limiting its
   size might prevent large programs from executing. Its size is only limited
   by the growth of the data space. It is initialized just below the maximum
   span of the parameter stack (7C00h), and it can grows down until the return stack
   pointer hits HERE.
   
   To push to the return stack, we store at 0,Y then dey dey (post-decrement)
   To pop from return stack, we increment Y by 2 with ldab #2 aby then load at 0,y (pre-increment)
   Top of stack can be peeked by index addressing using an offset of 2.
   For the moment underflow and overflows are not detected.

   Interpreter
   -----------
   The intepreter receives chars from the input device until an end of line is
   reached. The compiler is then executed, which translates input to an unnamed
   word. The unnamed word is then executed.

   Compilation
   -----------
   Each word is recognized and replaced by the address of its code pointer.
   Note this is not the address of the definition but the code pointer itself.
   The last code pointer entry of a compiled word is a RETURN, which pops an
   address from the return stack and use it as new instruction pointer.

   The compiler uses all the CPU registers, so the previous contents of the
   forth registers is saved before compilation and restored after.

*/

	/* System definitions */
	.equ INIT , 0x3D
	/* Serial definitions */
	.equ BAUD , 0x2B
	.equ SCCR1, 0x2C
	.equ SCCR2, 0x2D
	.equ SCSR , 0x2E
	.equ SCDR , 0x2F
	.equ SCSR_RDRF     ,0x20 /* Receive buffer full */
	.equ SCSR_TDRE     ,0x80 /* Transmit buffer empty */


	/* Forth config */
	.equ	TIB_LEN, 80

	/* Define variables in internal HC11 RAM */
	.data
IP:	.word	0	/* Instruction pointer */
HEREP:	.word	0	/* Pointer to HERE, the address of next free byte in dic/data space */
LASTP:	.word	0	/* Pointer to the last defined word or name */
BASE:	.word	0	/* Value of the base used for number parsing */
BEHAP:  .word   0       /* Pointer to word that implements the current behaviour: compile/interpret */

	/* Input text buffering */

NTIB:	.word	0	/* Number of characters in current line */
TIB:	.space	TIB_LEN	/* Input buffer */
IN:	.word	0	/* Parse position in the input buffer */



/*===========================================================================*/
/* Structure of a compiled word: We have a suite of code pointers. Each code pointer has to be executed in turn.
 * The last code pointer of the list has to be "exit", which returns to the caller, or a loop.
 * +---+------------+------+------+------+-----+------+
 * |HDR| code_ENTER | PTR1 | PTR2 | PTR3 | ... | EXIT |
 * +---+------------+------+------+------+-----+------+
 *                     ^      ^           
 *                     IP    nxtIP=IP+2    
 * IP ONLY POINTS AT WORD ADDRESSES! Never to asm code addresses. Thats why
   words implemented in assembly are only made of a code pointer.
 */

	.text
/*===========================================================================*/
/* Startup code */
/*===========================================================================*/
	.globl _start
_start:

	/* Map registers in zero page */
	clra
	staa	INIT+0x1000

	/* Init serial port */

	ldaa	#0x30
	staa	BAUD
	ldaa	#0x0C
	staa	SCCR2

	/* Init default values */
	ldx	#10
	stx	*BASE

	ldx	#TIB_LEN
	stx	*NTIB

	ldx	#word_QUIT
	stx	*LASTP

	/* Setup the runtime environment */

	lds	#(0x8000-1)	/* Parameter stack at end of RAM. HC11 pushes byte per byte. */
	ldy	#(0x7C00-2)	/* Return stack 1K before end of RAM. We push word per word. */
	ldx	#QUIT2		/* load pointer to startup code, skipping the native ENTER pointer! */
	bra	NEXT2		/* Start execution */

/*===========================================================================*/
/* Core routines for word execution */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
/* Execute the next word. IP is incremented, stored back, and the cell pointed
 * by IP is loaded. It contains a code address, which is jumped at.
 * This is not a forth word.
 */
	.text
	.globl PUSHD /* ensure GNU as makes this symbol visible */
PUSHD:
	pshb			/* We can use this instead of next to push a result before ending a word */
	psha
NEXT:
	ldx	*IP		/* Get the instruction pointer */
NEXT2:				/* We can call here if X already has the value for IP */
	inx			/* Increment IP to look at next word */
	inx
	stx	*IP		/* Save IP for next execution round */
	dex			/* Redecrement, because we need the original IP */
	dex			/* Now X contains pointer to pointer to code (aka IP, pointer to forth opcode) */
	ldx	0,X		/* Now X contains pointer to code (forth opcode == address of first cell in any word) */
	ldd	0,X		/* Now D contains the code address to execute opcode (a code_THING value) */
	xgdx			/* Switch, so X contains the code_THING value for this forth word 
				   and D contains address of instruction being run */
	jmp	0,X		/* Call the code that must run now, D containing the forth opcode being run */

/*---------------------------------------------------------------------------*/
/* Starts execution of a compiled word. The current IP is pushed on the return stack, then we jump */
/* This function is always called by the execution of NEXT. */
code_ENTER:
	/* This is called with the address of instruction being run (aka forth opcode) */
	ldx	*IP
	stx	0,Y		/* Push the next IP to be executed after return from this thread */
	dey			/* Post-Decrement Y by 2 to push */
	dey
	xgdx			/* Put enter opcode address in X */
	inx			/* Increment, now X is the address of the first word in the list */
	inx
	bra	NEXT2		/* Manage text opcode address */

/*---------------------------------------------------------------------------*/
/* Exit ends the execution of a word. The previous IP is on the return stack, so we pull it */
RETURN:
	.word	code_RETURN
code_RETURN:
	ldab	#2
	aby			/* Pre-Increment Y by 2 to pop */
	ldx	0,Y		/* Pop previous value for IP from top of return stack */
	bra	NEXT2

/*===========================================================================*/
/* Internal words */
/* These words have no header because they cannot be executed by the user.
 * However, they are used to implement compiled routines.
 */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
/* Do litteral: Next cell in thread is a litteral value to be pushed. */
	.section .rodata
LITTERAL:
	.word	code_LITTERAL
	.text
code_LITTERAL:
	ldx	*IP	/* Load next word in D */
	ldd	0,X
	inx		/* Increment IP to look at next word */
	inx
	stx	*IP	/* IP+1->IP Save IP for next execution */
	bra	PUSHD

/*---------------------------------------------------------------------------*/
/* Load next word in IP */
	.section .rodata
BRANCH:
	.word	code_BRANCH
	.text
code_BRANCH:
	ldx	*IP	/* Load next word in D */
	ldx	0,X
	bra	NEXT2

/*---------------------------------------------------------------------------*/
/* Pull a value. If zero, load next word in IP */
	.section .rodata
BRANCHZ:
	.word	code_BRANCHZ
	.text
code_BRANCHZ:
	ldx	*IP	/* Load next word in D */
	ldd	0,X
	inx
	inx
	stx	*IP

	pulx		/* Get flag */
	cpx	#0x0000 /* TODO make it more efficient */
	beq	qbranch1
	bra	NEXT	/* Not zero */

qbranch1: /* Pulled value has zero, do the branch */
	xgdx /* get the next word in X, then execute it */
	bra	NEXT2

/*---------------------------------------------------------------------------*/
/* Pull a value. Decrement and push, jump if zero */
	.section .rodata
DJNZ:
	.word	code_DJNZ
	.text
code_DJNZ:
	bra	NEXT

/*===========================================================================*/
/* Native words */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
/* Execute the code whose address is on the stack - likely wrong in this state */
	.section .dic
word_EXECUTE:
	.word	0
	.asciz "EXECUTE"
EXECUTE:
	.word	code_EXECUTE

	.text
code_EXECUTE:
	pulx		/* Retrieve a word address from stack */
	ldx	0,X	/* Get the code pointer stored at this address */
	jmp	0,X	/* Launch it */


/*---------------------------------------------------------------------------*/
/* Store a cell at address (d a -- ) */
	.section .dic
word_STORE:
	.word	word_EXECUTE
	.asciz	"!"
STORE:
	.word	code_STORE

	.text
code_STORE:
	pulx	/* TOS contains address */
	pula	/* PREV contains data */
	pulb
	std	0,X
	bra	NEXT

/*---------------------------------------------------------------------------*/
/* Store a char at address (c a -- ) */
	.section .dic
word_CSTORE:
	.word	word_EXECUTE
	.asciz	"C!"
CSTORE:
	.word	code_CSTORE

	.text
code_CSTORE:
	pulx	/* TOS contains address */
	pula	/* PREV contains data, A = MSB */
	pulb	/* B = LSB */
	stab	0,X
	bra	NEXT

/*---------------------------------------------------------------------------*/
/* Load a cell at given address (a -- d) */
	.section .dic
word_LOAD:
	.word	word_CSTORE
	.asciz "@"
LOAD:
	.word	code_LOAD

	.text
code_LOAD:
	pulx
	ldd	0,X
	bra	PUSHD

/*---------------------------------------------------------------------------*/
/* Load a cell at given address (a -- d) */
	.section .dic
word_CLOAD:
	.word	word_CLOAD
	.asciz "C@"
CLOAD:
	.word	code_CLOAD

	.text
code_CLOAD:
	pulx
	clra
	ldab	0,X
	bra	PUSHD

/*---------------------------------------------------------------------------*/
/* R> ( -- x ) ( R: x -- ) */
	.section .dic
word_RFROM:
	.word word_CLOAD
	.asciz "R>"
RFROM:
	.word code_RFROM

	.text
code_RFROM:
	ldab	#2	/* Preinc Y to pull from Return Stack */
	aby
	ldd	0,Y
	bra	PUSHD

/*---------------------------------------------------------------------------*/
/* >R ( -- x ) ( R: x -- ) */
	.section .dic
word_TOR:
	.word	word_RFROM
	.asciz	">R"
TOR:
	.word	code_TOR

	.text
code_TOR:
	pula
	pulb
	std	0,Y
	dey		/* Postdec Y to push on Return Stack */
	dey
	bra	NEXT

/*---------------------------------------------------------------------------*/
/* DUP ( u -- u u ) */
	.section .dic
word_DUP:
	.word	word_TOR
	.asciz "DUP"
DUP:
	.word	code_DUP

	.text
code_DUP:
	tsx			/* Get stack pointer +1 in X */
	ldd	1,X		/* Load top of stack in D */
	bra	NEXT		/* This will push top of stack again */

/*---------------------------------------------------------------------------*/
/* ( u1 u2 -- u1 u2 u1 ) */
	.section .dic
word_OVER:
	.word	word_DUP
	.asciz "OVER"
OVER:
	.word	code_OVER

	.text
code_OVER:
	tsx			/* Get stack pointer +1 in X */
	ldd	3,X		/* Load value before top of stack */
	bra	PUSHD

/*---------------------------------------------------------------------------*/
/* SWAP ( u v -- v u ) */
	.section .dic
word_SWAP:
	.word	word_OVER
	.asciz "SWAP"
SWAP:
	.word	code_SWAP

	.text
code_SWAP:
	pulx
	pula
	pulb
	pshx
	bra	PUSHD		/* This will push top of stack again */

/*---------------------------------------------------------------------------*/
/* ( u -- ) */
	.section .dic
word_DROP:
	.word	word_SWAP
	.asciz "DROP"
DROP:
	.word	code_DROP

	.text
code_DROP:
	pulx			/* Get a parameter and discard it */
	bra	NEXT		/* This will push top of stack again */

/* Math */

/*---------------------------------------------------------------------------*/
/* ( u v -- u+v ) */
	.section .dic
word_PLUS:
	.word	word_DROP
	.asciz	"+"
PLUS:
	.word	code_PLUS

	.text
code_PLUS:
	pula
	pulb
	tsx
	addd	1,X
	bra	PUSHD

/*---------------------------------------------------------------------------*/
/* ( u v -- u^v ) */
	.section .dic
word_XOR:
	.word	word_PLUS
	.asciz	"+"
XOR:
	.word	code_XOR

	.text
code_XOR:
	pula
	pulb
	tsx
	eora	1,X
	eorb	2,X
	bra	PUSHD

/*---------------------------------------------------------------------------*/
/* ( u -- u<0 ) push true if pull negative */
	.section .dic
word_ZLESS:
	.word	word_XOR
	.asciz	"0<"
ZLESS:
	.word	code_ZLESS

	.text
code_ZLESS:
	pula
	pulb
	tsta		/* check high bit of MSB */
	bmi	.Ltrue	/* branch if negative */
	ldd     #0x0000
	bra	PUSHD
.Ltrue:
	ldd     #0xFFFF
	bra	PUSHD

/*===========================================================================*/
/* Native IO */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
	.section .dic
word_EMIT:
	.word	word_XOR
	.asciz	"EMIT"
EMIT:
	.word	code_EMIT

	.text
code_EMIT:
	pula
	pulb
.Lemit2:
	brclr	*SCSR #SCSR_TDRE, .Lemit2
	stab	*SCDR
	bra	NEXT

/*---------------------------------------------------------------------------*/
	.section .dic
word_KEY:
	.word	word_EMIT
	.asciz	"KEY"
KEY:
	.word	code_KEY

	.text
code_KEY:
	brclr	*SCSR #SCSR_RDRF, code_KEY
	ldab	*SCDR
	clra
	bra	PUSHD

/*===========================================================================*/
/* Other forth words implemented in forth.
 * These words are pre-compiled lists, they are all executed by code_ENTER.
 * The following words can only be pointers to cells containing references to
 * other words. Direct pointers to cells containing code addresses are not
 * possible.
 */
/*===========================================================================*/

/*===========================================================================*/
/* Basic ops */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
/* DDUP ( u1 u2 -- u1 u2 u1 u2 ) */
	.section .dic
word_DDUP:
	.word	word_KEY
	.asciz "2DUP"
DDUP:
	.word	code_ENTER
	.word	OVER
	.word	OVER
	.word	RETURN

/*===========================================================================*/
/* Math */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
	.section .dic
word_NOT:
	.word	word_DDUP
	.asciz	"NOT"
NOT:
	.word	code_ENTER
	.word	LITTERAL, 0xFFFF
	.word	XOR
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( u u -- t ) unsigned compare of top two items. */

	.section .dic
word_ULESS:
	.word	word_NOT
	.asciz	"U<"
ULESS:
	.word	code_ENTER
	.word	DDUP
	.word	XOR
	.word	ZLESS
	.word	BRANCHZ, ULESS1
	.word	SWAP
	.word	DROP
	.word	ZLESS
	.word	RETURN
ULESS1:
	.word	SUB
	.word	ZLESS
	.word	RETURN

/*---------------------------------------------------------------------------*/
	.section .dic
word_NEGATE:
	.word	word_ULESS
	.asciz	"NEGATE"
NEGATE:
	.word	code_ENTER
	.word	NOT
	.word	LITTERAL, 1
	.word	PLUS
	.word	RETURN

/*---------------------------------------------------------------------------*/
	.section .dic
word_SUB:
	.word word_NEGATE
	.asciz	"-"
SUB:
	.word	code_ENTER
	.word	NEGATE
	.word	PLUS
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( u -- u+2 ) */
	.section .dic
word_CELLP:
	.word	word_SUB
	.asciz "CELL+"
CELLP:
	.word	code_ENTER
	.word	LITTERAL,2
	.word	PLUS
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( u -- u+1 ) */
	.section .dic
word_CHARP:
	.word	word_CELLP
	.asciz "CHAR+"
CHARP:
	.word	code_ENTER
	.word	LITTERAL,1
	.word	PLUS
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( u ul uh -- ul <= u < uh ) */
	.section .dic
word_WITHIN:
	.word	word_CHARP
	.asciz	"WITHIN"
WITHIN:
	.word	code_ENTER
	.word	OVER
	.word	SUB
	.word	TOR
	.word	SUB
	.word	RFROM
	.word	ULESS
	.word	RETURN

/*===========================================================================*/
/* Strings and chars */
/*===========================================================================*/
/* ( -- 32 ) */

	.section .dic
word_BL:
	.word	word_CELLP
	.asciz	"BL"
BL:
	.word	code_ENTER
	.word	LITTERAL, 32
	.word	RETURN

/*===========================================================================*/
/* State control */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
/* Set the system state to interpretation */
	.section .dic
word_INTERP:
	.word	word_BL
	.asciz	"["
INTERP:
	.word	code_ENTER
	.word	LITTERAL, EXECUTE
	.word	LITTERAL, BEHAP
	.word	STORE
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* Set the system state to compilation */
	.section .dic
word_STARTCOMP:
	.word	word_INTERP
	.asciz	"]"
STARTCOMP:
	.word	code_ENTER
	.word	LITTERAL
	.word	COMPILE
	.word	LITTERAL
	.word	BEHAP
	.word	STORE
	.word	RETURN

/*===========================================================================*/
/* Memory management */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
/* Push the address of the next free byte ( -- a) */
word_HERE:
	.word	word_STARTCOMP
	.asciz	"HERE"
HERE:
	.word	code_ENTER
	.word	LITTERAL
	.word	HEREP		/* (HEREP=&HERE) */
	.word	LOAD		/* (HERE) */ 
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* (u -- ) Pop a word and save it HERE, then make HERE point to the next cell */
	.section .dic
word_COMMA:
	.word	word_HERE
	.asciz	","
COMMA:
	.word	code_ENTER
	.word	HERE		/* (VALUE) (HERE) */
	.word	DUP		/* (VALUE) (HERE) (HERE) */
	.word	CELLP		/* (VALUE) (HERE) (HERE+2) */
	.word	LITTERAL, HEREP	/* (VALUE) (HERE) (HERE+2) (HEREP=&HERE) */
	.word	STORE		/* (VALUE) (HERE) */
	.word	STORE		/* Empty */
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* (u -- ) Pop a word and the LSB char it HERE, then make HERE point to the next cell */
	.section .dic
word_CCOMMA:
	.word	word_COMMA
	.asciz	"C,"
CCOMMA:
	.word	code_ENTER
	.word	HERE		/* (VALUE) (HERE) */
	.word	DUP		/* (VALUE) (HERE) (HERE) */
	.word	CHARP		/* (VALUE) (HERE) (HERE+1) */
	.word	LITTERAL
	.word	HEREP		/* (VALUE) (HERE) (HERE+1) (HP=&HERE) */
	.word	STORE		/* (VALUE) (HERE) */
	.word	CSTORE		/* Empty */
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* (u -- ) store u, actually similar to COMMA */
	.section .dic
word_COMPILE:
	.word	word_CCOMMA
	.asciz	"COMPILE,"
COMPILE:
	.word	code_ENTER
	.word	COMMA
	.word	RETURN
	
/*---------------------------------------------------------------------------*/
/* ( a u -- a u) Read up to TIB_LEN or EOL into the TIB.
   Return buf and char count */
	.section .dic
word_ACCEPT:
	.word	word_COMPILE
	.asciz "ACCEPT"
ACCEPT:
	.word	code_ENTER
	.word	OVER
	.word	PLUS
	.word	OVER
ACCEPT1:
	.word	DDUP
	.word	XOR
	.word	BRANCHZ,ACCEPT4
	.word	KEY
	.word	DUP
	.word	BL,LITTERAL,127,WITHIN
	.word	BRANCHZ,ACCEPT2
	.word	TAP
	.word	BRANCH,ACCEPT3
ACCEPT2:
	.word	TTAP
ACCEPT3:
	.word	BRANCH,ACCEPT1
ACCEPT4:
	.word	DROP
	.word	OVER
	.word	SUB
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* Main forth interactive interpreter loop */
	.section .dic
word_QUIT:
	.word	word_ACCEPT
	.asciz "QUIT"
QUIT:
	.word	code_ENTER
QUIT2:
	.word	LITTERAL, TIB
	.word	LITTERAL, NTIB
	.word	LOAD
	.word	ACCEPT
	.word	BRANCH, QUIT2
	.word	RETURN /* Unreached */


