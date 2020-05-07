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
   BASE - Contains the value of the current number conversion base
   
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
LASTP:	.word	0	/* Pointer to the last defined word entry */
BASEP:	.word	0	/* Value of the base used for number parsing */
HOLDP:	.word	0	/* Pointer used for numeric output */
BEHAP:  .word   0       /* Pointer to word that implements the current behaviour: compile/interpret */
HANDP:	.word	0	/* Exception handler pointer */

	/* Input text buffering */

pTEMP:	.word	0	/* Temp variable used in LPARSE */
INN:	.word	0	/* Parse position in the input buffer */
NTIB:	.word	0	/* Number of characters in current line */
TIB:	.space	TIB_LEN	/* Input buffer */



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
	staa	*BAUD
	ldaa	#0x0C
	staa	*SCCR2

	ldx	#word_BOOT
	stx	*LASTP

	/* Setup the runtime environment */
	ldx	#(0x100)
	stx	*HEREP		/*Define HERE, the beginning of the user data zone */

	lds	#(0x8000-1)	/* Parameter stack at end of RAM. HC11 pushes byte per byte. */
	ldy	#(0x7C00-2)	/* Return stack 1K before end of RAM. We push word per word. */
	ldx	#BOOT1		/* load pointer to startup code, skipping the native ENTER pointer! */
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
doEXECUTE:
	ldd	0,X		/* Now D contains the code address to execute opcode (a code_THING value) */
	pshb
	psha
	rts			/* Call the code_THING address pushed on stack, X contains new IP */

/*---------------------------------------------------------------------------*/
/* Starts execution of a compiled word. The current IP is pushed on the return stack, then we jump */
/* This function is always called by the execution of NEXT. */
code_ENTER:
	/* This is called with the address of instruction being run (aka forth opcode) in D*/
	ldd	*IP
	std	0,Y		/* Push the next IP to be executed after return from this thread */
	dey			/* Post-Decrement Y by 2 to push */
	dey
	inx			/* Increment, now X is the address of the first word in the list */
	inx
	bra	NEXT2		/* Manage next opcode address */

/*---------------------------------------------------------------------------*/
/* Exit ends the execution of a word. The previous IP is on the return stack, so we pull it */
	.section .rodata
RETURN:
	.word	code_RETURN
	.text
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
/* Do litteral: Next cell in thread is an immediate litteral value to be pushed. */
	.section .rodata
IMM:
	.word	code_IMM
	.text
code_IMM:
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
	ldx	*IP	/* Load next word in X */
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
	ldd	0,X	/* D contains branch destination */
	inx
	inx
	stx	*IP	/* Make IP look at next word after branch address */

	pulx		/* Get flag */
	cpx	#0x0000 /* TODO make it more efficient */
	beq	qbranch1
	bra	NEXT	/* Not zero */

qbranch1: /* Pulled value was zero, do the branch */
	xgdx /* store branch destination (D) in X, then execute at this point */
	bra	NEXT2

/*---------------------------------------------------------------------------*/
/* Pull a value from R stack. Decrement and push, jump if zero */
	.section .rodata
JNZD:
	.word	code_JNZD
	.text
code_JNZD:
	ldd	2,y		/* get counter on return stack */
	beq	.Lbranch	/* branch if loop is done (index is zero) */
	subd	#1		/* no, bump the counter */
	std	2,y		/* and replace on stack */
	bra	code_BRANCH	/* branch to target using existing code */
.Lbranch:
	iny
	iny			/* done, burn counter from stack */
	ldx	*IP		/* get the IP */
	inx
	inx			/* and get addr past branch target */
	bra	NEXT2		/* and go do next word */

/*===========================================================================*/
/* Native words */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
/* Execute the code whose address is on the stack */
	.section .dic
word_EXECUTE:
	.word	0
	.byte	7
	.ascii	"EXECUTE"
EXECUTE:
	.word	code_EXECUTE

	.text
code_EXECUTE:
	pulx		/* Retrieve a word address from stack. This address contains a code pointer */
	bra	doEXECUTE

/*---------------------------------------------------------------------------*/
/* Store a cell at address (d a -- ) */
	.section .dic
word_STORE:
	.word	word_EXECUTE
	.byte	1
	.ascii	"!"
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
	.word	word_STORE
	.byte	2
	.ascii	"C!"
CSTORE:
	.word	code_CSTORE

	.text
code_CSTORE:
	pulx	/* TOS contains address */
	pula	/* PREV contains data, A = MSB, discarded */
	pulb	/* B = LSB */
	stab	0,X
	bra	NEXT

/*---------------------------------------------------------------------------*/
/* Load a cell at given address (a -- d) */
	.section .dic
word_LOAD:
	.word	word_CSTORE
	.byte	1
	.ascii	"@"
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
	.word	word_LOAD
	.byte	2
	.ascii	"C@"
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
	.byte	2
	.ascii	"R>"
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
	.byte	2
	.ascii	">R"
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
/* @R ( -- x ) ( R: x -- x ) */
	.section .dic
word_RLOAD:
	.word	word_TOR
	.byte	2
	.ascii	"@R"
RLOAD:
	.word	code_RLOAD

	.text
code_RLOAD:
	ldd	2,Y
	bra	PUSHD

/*---------------------------------------------------------------------------*/
	.section .dic
word_SPLOAD:
	.word	word_RLOAD
	.byte	3
	.ascii	"SP@"
SPLOAD:
	.word	code_SPLOAD
	.text
code_SPLOAD:
	tsx
	dex
	pshx
	bra	NEXT

/*---------------------------------------------------------------------------*/
	.section .dic
word_SPSTORE:
	.word	word_SPLOAD
	.byte	3
	.ascii	"SP!"
SPSTORE:
	.word	code_SPSTORE
	.text
code_SPSTORE:
	pulx
	inx
	txs
	bra	NEXT

/*---------------------------------------------------------------------------*/
	.section .dic
word_RPLOAD:
	.word	word_SPSTORE
	.byte	3
	.ascii	"RP@"
RPLOAD:
	.word	code_RPLOAD
	.text
code_RPLOAD:
	pshy
	bra	NEXT

/*---------------------------------------------------------------------------*/
	.section .dic
word_RPSTORE:
	.word	word_RPLOAD
	.byte	3
	.ascii	"RP!"
RPSTORE:
	.word	code_RPSTORE
	.text
code_RPSTORE:
	puly
	bra	NEXT

/*---------------------------------------------------------------------------*/
/* DUP ( u -- u u ) */
	.section .dic
word_DUP:
	.word	word_RPSTORE
	.byte	3
	.ascii	"DUP"
DUP:
	.word	code_DUP

	.text
code_DUP:
	tsx			/* Get stack pointer +1 in X */
	ldd	0,X		/* Load top of stack in D */
	bra	PUSHD		/* This will push top of stack again */

/*---------------------------------------------------------------------------*/
/* ( u1 u2 -- u1 u2 u1 ) */
	.section .dic
word_OVER:
	.word	word_DUP
	.byte	4
	.ascii	"OVER"
OVER:
	.word	code_OVER

	.text
code_OVER:
	tsx			/* Get stack pointer +1 in X */
	ldd	2,X		/* Load value before top of stack */
	bra	PUSHD

/*---------------------------------------------------------------------------*/
/* SWAP ( u v -- v u ) */
	.section .dic
word_SWAP:
	.word	word_OVER
	.byte	4
	.ascii	"SWAP"
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
	.byte	4
	.ascii	"DROP"
DROP:
	.word	code_DROP

	.text
code_DROP:
	pulx			/* Get a parameter and discard it */
	bra	NEXT		/* This will push top of stack again */

/*===========================================================================*/
/* Math */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
/* ( u v -- u+v cy ) */
	.section .dic
word_UPLUS:
	.word	word_DROP
	.byte	3
	.ascii	"UM+"
UPLUS:
	.word	code_UPLUS

	.text
code_UPLUS:
	pula
	pulb		/*pull TOS*/
	tsx
	addd	0,X	/*add to new TOS, sets N,Z,C,V */
	std	0,X	/*Replace TOS, does not affect carry, clears V, changes N and Z */
	ldab	#0	/*CLRB clears carry, LDAB leaves it.*/
	rolb		/*Get carry flag in B0 (D.LSB)*/
	clra		/*Clear A (D.MSB)*/
	bra	PUSHD	/* Push second return item and do next word */

/*---------------------------------------------------------------------------*/
/* ( u v -- u+v ) : + UM+ DROP ; */
/* We do a native version for speed */
	.section .dic
word_PLUS:
	.word	word_UPLUS
	.byte	1
	.ascii	"+"
PLUS:
	.word	code_PLUS

	.text
code_PLUS:
	pula
	pulb
	tsx
	addd	0,X
	std	0,X
	bra	NEXT

/*---------------------------------------------------------------------------*/
/* ( u v -- u^v ) */
	.section .dic
word_XOR:
	.word	word_PLUS
	.byte	3
	.ascii	"XOR"
XOR:
	.word	code_XOR

	.text
code_XOR:
	pula
	pulb
	tsx
	eora	0,X
	eorb	1,X
	pulx
	bra	PUSHD

/*---------------------------------------------------------------------------*/
/* ( u v -- u&v ) */
	.section .dic
word_AND:
	.word	word_XOR
	.byte	3
	.ascii	"AND"
AND:
	.word	code_AND

	.text
code_AND:
	pula
	pulb
	tsx
	anda	0,X
	andb	1,X
	pulx
	bra	PUSHD

/*---------------------------------------------------------------------------*/
/* ( u v -- u|v ) */
	.section .dic
word_OR:
	.word	word_AND
	.byte	2
	.ascii	"OR"
OR:
	.word	code_OR

	.text
code_OR:
	pula
	pulb
	tsx
	oraa	0,X
	orab	1,X
	pulx
	bra	PUSHD

/*---------------------------------------------------------------------------*/
/* ( u -- u<0 ) push true if pull negative */
	.section .dic
word_ZLESS:
	.word	word_OR
	.byte	2
	.ascii	"0<"
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
	.word	word_ZLESS
	.byte	4
	.ascii	"EMIT"
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
	.byte	3
	.ascii	"KEY"
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
/*   pad       ( -- a ) - return the address of a temporary buffer. */
/* Note: this returns the END of a 80 byte buffer right after the current colon definition.
   The buffer is filled in reverse using a div/mod by base algorithm.
   No overflow because numeric output is never overlapping compilation. PAD is always used
   in the context defined by <# and #> */
	.section .dic
word_PAD:
	.word	word_KEY
	.byte	3
	.ascii	"PAD"
PAD:
	.word	code_ENTER
	.word	HERE
	.word	IMM,80
	.word	PLUS
	.word	RETURN

/*---------------------------------------------------------------------------*/
/*( wordptr -- ) Execute the forth word whose address is stored in the passed pointer */
	.section .dic
/* NO NAME */
LOADEXEC:
	.word	code_ENTER	/* ptr */
	.word	LOAD		/* word */
	.word	DUP		/* word word */
	.word	BRANCHZ, noexec /* word, exit if null */
	.word	EXECUTE		/* Execute the loaded forth word*/
noexec:
	.word	RETURN		/* Nothing is stored. just return. */

/*---------------------------------------------------------------------------*/
/* DDUP ( u1 u2 -- u1 u2 u1 u2 ) */
	.section .dic
word_DDUP:
	.word	word_PAD
	.byte	4
	.ascii	"2DUP"
DDUP:
	.word	code_ENTER
	.word	OVER
	.word	OVER
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( x x -- ) */
	.section .dic
word_DDROP:
	.word	word_DDUP
	.byte	5
	.ascii	"2DROP"
DDROP:
	.word	code_ENTER
	.word	DROP
	.word	DROP
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* DUPNZ ( u -- u u if u NOT zero ) */
	.section .dic
word_DUPNZ:
	.word	word_DDROP
	.byte	4
	.ascii	"?DUP"
DUPNZ:
	.word	code_ENTER
	.word	DUP		/* Dup first, to allow testing */
	.word	BRANCHZ,DUPNZ2	/* if zero, no dup happens (this consumes the first dupe) */
	.word	DUP		/* Not zero: Dup the value and leave on stack */
DUPNZ2:
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ROT ( a b c -- b c a ) */
	.section .dic
word_ROT:
	.word	word_DUPNZ
	.byte	3
	.ascii	"ROT"
ROT:
	.word	code_ENTER
	.word	TOR
	.word	SWAP
	.word	RFROM
	.word	SWAP
	.word	RETURN

/*===========================================================================*/
/* Math and logical */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
	.section .dic
word_NOT:
	.word	word_ROT
	.byte	3
	.ascii	"NOT"
NOT:
	.word	code_ENTER
	.word	IMM, 0xFFFF
	.word	XOR
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( u -- (-u) ) NEGATE: Twos complement */
	.section .dic
word_NEGATE:
	.word	word_NOT
	.byte	6
	.ascii	"NEGATE"
NEGATE:
	.word	code_ENTER
	.word	NOT
	.word	IMM, 1
	.word	PLUS
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( a b -- a-b ) - could be assembly to improve performance a bit */
	.section .dic
word_SUB:
	.word word_NEGATE
	.byte	1
	.ascii	"-"
SUB:
	.word	code_ENTER
	.word	NEGATE
	.word	PLUS
	.word	RETURN

/*---------------------------------------------------------------------------*/
	.section .dic
word_ABS:
	.word	word_SUB
	.byte	3
	.ascii	"ABS"
ABS:
	.word	code_ENTER
	.word	DUP
	.word	ZLESS
	.word	BRANCHZ,abspos
	.word	NEGATE	
abspos:
	.word	RETURN

/*---------------------------------------------------------------------------*/
/*( u v -- uvlo uvhi ) - long 16x32 multiplication */
/*TODO native implementation since hc11 has a multiplier */
	.section .dic
word_UMSTAR:
	.word	word_ABS
	.byte	3
	.ascii	"UM*"
UMSTAR:
	.word	code_ENTER
	.word	IMM,0,SWAP,IMM,15,TOR
umst1:	.word	DUP,UPLUS,TOR,TOR
	.word	DUP,UPLUS,RFROM,PLUS,RFROM
	.word	BRANCHZ,umst2
	.word	TOR,OVER,UPLUS,RFROM,PLUS
umst2:	.word	JNZD,umst1
	.word	ROT,DROP
	.word	RETURN

/*---------------------------------------------------------------------------*/
/*   um/mod    ( udl udh u -- ur uq ) 32/16 division and modulo */
/*TODO native implementation since hc11 has a divider*/
	.section .dic
word_UMMOD:
	.word	word_UMSTAR
	.byte	6
	.ascii	"UM/MOD"
UMMOD:
	.word	code_ENTER
	.word	DDUP
	.word	ULESS
	.word	BRANCHZ,umm4
	.word	NEGATE
	.word	IMM,15
	.word	TOR
umm1:
	.word	TOR
	.word	DUP
	.word	UPLUS
	.word	TOR,TOR,DUP,UPLUS
	.word	RFROM,PLUS,DUP
	.word	RFROM,RLOAD,SWAP,TOR
	.word	UPLUS,RFROM,OR
	.word	BRANCHZ,umm2
	.word	TOR,DROP,IMM,1,PLUS,RFROM
	.word	BRANCH,umm3
umm2:
	.word	DROP
umm3:
	.word	RFROM
	.word	JNZD,umm1
	.word	DROP,SWAP,RETURN
umm4:
	.word	DROP,DDROP
	.word	IMM,-1,DUP		/* overflow, return max*/
	.word	RETURN

/*---------------------------------------------------------------------------*/
/*( u v -- u*v ) - short 16x16 multiplication with result same size as operands. we just drop half of the result bits */
	.section .dic
word_STAR:
	.word	word_UMMOD
	.byte	1
	.ascii	"*"
STAR:
	.word	code_ENTER
	.word	UMSTAR
	.word	DROP		/*Forget the Most Significant word */
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( u -- u+2 ) */
	.section .dic
word_CELLP:
	.word	word_STAR
	.byte	5
	.ascii "CELL+"
CELLP:
	.word	code_ENTER
	.word	IMM,2
	.word	PLUS
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( u -- u+1 ) */
	.section .dic
word_CHARP:
	.word	word_CELLP
	.byte	5
	.ascii "CHAR+"
CHARP:
	.word	code_ENTER
	.word	IMM,1
	.word	PLUS
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( u -- u*2 ) - compute bytes required to store u cells */
	.section .dic
word_CELLS:
	.word	word_CHARP
	.byte	5
	.ascii "CELLS"
CELLS:
	.word	code_ENTER
	.word	DUP
	.word	PLUS
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( u v -- u<v ) unsigned compare of top two items. */
	.section .dic
word_ULESS:
	.word	word_CELLS
	.byte	2
	.ascii	"U<"
ULESS:
	.word	code_ENTER
	.word	DDUP		/* (u) (v) (u)     (v) */
	.word	XOR		/* (u) (v) (u^v)      */
	.word	ZLESS		/* (u) (v) ((u^v)<0) */
	.word	BRANCHZ, ULESS1
	.word	SWAP		/* (v) (u) */
	.word	DROP		/* (v)    */
	.word	ZLESS		/* (v<0) */
	.word	RETURN
ULESS1:
	.word	SUB		/* (u-v) */
	.word	ZLESS		/* ((u-v)<0) */
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( n1 n2 -- t ) - signed compare of top two items. */
	.section .dic
word_LESS:
	.word	word_ULESS
	.byte	1
	.ascii	"<"
LESS:
	.word	code_ENTER
	.word	DDUP
	.word	XOR
	.word	ZLESS
	.word	BRANCHZ,less1
	.word	DROP
	.word	ZLESS
	.word	RETURN
less1:
	.word	SUB
	.word	ZLESS
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( u ul uh -- ul <= u < uh ) */
	.section .dic
word_WITHIN:
	.word	word_LESS
	.byte	6
	.ascii	"WITHIN"
WITHIN:
	.word	code_ENTER
	.word	OVER		/*u ul uh ul*/
	.word	SUB		/*u ul (uh-ul) */
	.word	TOR		/*u ul R: (uh-ul) */
	.word	SUB		/*(u-ul) R: (uh-ul) */
	.word	RFROM		/* (u-ul) (uh-ul) */
	.word	ULESS		/* ((u-ul) < (uh-ul)) */
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( w w -- t ) equality flag FFFF if both value are the same (xor would return zero for equality)*/
word_EQUAL:
	.word	word_WITHIN
	.byte	5
	.ascii	"EQUAL"
EQUAL:
	.word	code_ENTER
	.word	XOR
	.word	BRANCHZ,equtrue
	.word	IMM,0
	.word	RETURN
equtrue:
	.word	IMM,0xFFFF	/* True is -1 ! */
	.word	RETURN

/*===========================================================================*/
/* Strings */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
/* COUNT ( cstradr -- bufadr len ) Return the buf addr and len of a pointed counted string */
	.section .dic
word_COUNT:
	.word	word_EQUAL
	.byte	5
	.ascii	"COUNT"
COUNT:
	.word	code_ENTER
	.word	DUP		/* cstradr cstradr */
	.word	IMM,1		/* cstradr cstradr 1*/
	.word	PLUS		/* cstradr bufadr */
	.word	SWAP		/* bufadr cstradr */
	.word	CLOAD		/* bufadr len */
	.word	RETURN

/*---------------------------------------------------------------------------*/
/*(src dest count --) */
	.section .dic
word_CMOVE:
	.word	word_COUNT
	.byte	5
	.ascii	"CMOVE"
CMOVE:
	.word	code_ENTER
	.word	TOR		/*src dest | R:count*/
	.word	BRANCH,cmov2	/**/
cmov1:
	.word	TOR		/*src | R:count dest*/
	.word	DUP		/*src src | R: count dest*/
	.word	CLOAD		/*src data | R: count dest*/
	.word	RLOAD		/*src data dest | R: count dest*/
	.word	CSTORE		/*src | R: count dest*/
	.word	IMM,1		/*src 1 | R: count dest*/
	.word	PLUS		/*src+1 | R: count dest*/
	.word	RFROM		/*src+1 dest | R: count */
	.word	IMM,1		/*src+1 dest 1 | R: count*/
	.word	PLUS		/*src+1->src dest+1->dest | R: count*/
cmov2:
	.word	JNZD, cmov1	/*src dest | if count>0, count--, goto cmov1 */
	.word	DDROP		/*-- R: -- */
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* (buf len dest -- dest) Create a counted string in dest from len chars at buf */
	.section .dic
word_PACKS:
	.word	word_CMOVE
	.byte	5
	.ascii	"PACK$"
PACKS:
	.word	code_ENTER
	/*Save count */
	.word	DUP		/*buf len dest dest */
	.word	TOR		/*buf len dest | R: dest*/
	.word	DDUP		/*buf len dest len dest | R: dest*/
	.word	CSTORE		/*buf len dest | R: dest*/
	/*Copy string after count */
	.word	IMM,1		/*buf len dest 1 | R:dest*/
	.word	PLUS		/*buf len (dest+1) | R:dest*/
	.word	SWAP		/*buf (dest+1) len | R:dest*/
	.word	CMOVE		/*R:dest*/
	.word	RFROM		/*dest*/
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* csame (ptra ptrb len -- flag ) - compare strings on len bytes */
	.section .dic
word_CSAME:
	.word	word_PACKS
	.byte	6
	.ascii	"CSAME?"
CSAME:
	.word	code_ENTER
	.word	TOR		/*ptra ptrb | R:len*/
	.word	BRANCH,csame2
csame0:
	.word	OVER		/*ptra ptrb ptra | R:len*/
	.word	CLOAD		/*ptra ptrb chra | R:len*/
	.word	OVER		/*ptra ptrb chra ptrb | R:len*/
	.word	CLOAD		/*ptra ptrb chra chrb | R:len*/
	.word	SUB		/*ptra ptrb chrdiff | R:len*/
	.word	DUP		/*ptra ptrb chrdiff chrdiff | R:len*/
	.word	BRANCHZ,csame1	/*ptra ptrb chrdiff | R:len*/
	/* chars are different, we're done */
	.word	RFROM		/*ptra ptrb chrdiff len */
	.word	DROP		/*ptra ptrb chrdiff */
	.word	TOR		/*ptra ptrb | R: chrdiff*/
	.word	DDROP		/*R: chrdiff*/
	.word	RFROM		/*chrdiff*/
	.word	RETURN

csame1:
	/*both chars are similar. Increment pointers and loop - ptra ptrb chrdiff | R:len*/
	.word	DROP		/*ptra ptrb | R:len*/
	.word	TOR		/*ptra | R: len ptrb*/
	.word	CHARP		/*ptra+1 | R: len ptrb*/
	.word	RFROM		/*ptra+1 ptrb | R: len*/
	.word	CHARP		/*ptra+1 ptrb+1 | R: len*/

csame2:
	.word	JNZD, csame0	/*ptra+1 ptrb+1 | R: len if not null, else --*/
	/* If we reached this point then both strings are same */
	.word	DDROP
	.word	IMM,0
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ccompare (cstr cstr -- flag ) - return 0 if match */
	.section .dic
word_CCOMPARE:
	.word	word_CSAME
	.byte	8
	.ascii	"CCOMPARE"
CCOMPARE:
	.word	code_ENTER

	/*Compare lengths. not equal? not same strings.*/
	.word	OVER		/*cstra cstrb cstra*/
	.word	CLOAD		/*cstra cstrb lena*/
	.word	OVER		/*cstra cstrb lena cstrb*/
	.word	CLOAD		/*cstra cstrb lena lenb*/
	.word	SUB		/*cstra cstrb (lena-lenb)*/
	.word	DUP		/*cstra cstrb lendiff lendiff*/
	.word	TOR		/*cstra cstrb lendiff | R: lendiff*/
	.word	BRANCHZ, ccoeq	/*cstra cstrb | R:lendiff*/
	/* Different lengths */
	.word	DDROP		/*-- | R:lendiff*/
	.word	RFROM		/*lendiff*/
	.word	RETURN
	/*Length match. Compare chars */
ccoeq:
	.word	RFROM		/*cstra cstrb lendiff */
	.word	DROP		/*cstra cstrb*/
	.word	TOR		/*cstra / cstrb*/
	.word	COUNT		/*bufa cnta / cstrb */
	.word	RFROM		/*bufa cnta cstrb*/
	.word	CHARP		/*bufa cnta bufb*/
	.word	SWAP		/*bufa bufb cnta */
	.word	CSAME		/*result */
	.word	RETURN

/*---------------------------------------------------------------------------*/
/*   compare     ( buf1 len1 buf2 len2 -- flag ) */
/*   compare strings up to the length of the shorter string. zero if match */
/* TODO */

/*---------------------------------------------------------------------------*/
/* IMMSTR ( -- adr ) Push the address of an inline counted string that follows this word */
	.section .dic
/* NO NAME */
IMMSTR:
	.word	code_ENTER
	.word	RFROM		/* adr of next word -> points to length of inline counted string */
	.word	DUP		/* cstradr cstradr*/
	.word	COUNT		/* cstradr bufadr len */
	.word	PLUS		/* cstradr endadr */
	.word	TOR		/* cstradr R: nextwordadr */
	.word	RETURN

/*===========================================================================*/
/* Numeric output */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
/**/
	.section .dic
word_BDIGS:
	.word	word_CCOMPARE
	.byte	2
	.ascii	"<#"
BDIGS:
	.word	code_ENTER
	.word	PAD
	.word	IMM,HOLDP
	.word	STORE
	.word	RETURN

/*---------------------------------------------------------------------------*/
/*  #>    ( w -- b u ) - prepare the output string to be type'd. */
	.section .dic
word_EDIGS:
	.word	word_BDIGS
	.byte	2
	.ascii	"#>"
EDIGS:
	.word	code_ENTER
	.word	DROP
	.word	IMM,HOLDP
	.word	LOAD
	.word	PAD
	.word	OVER
	.word	SUB
	.word	RETURN

/*---------------------------------------------------------------------------*/
/*  hold ( c -- ) - insert a character into the numeric output string. Storage is predecremented. */
	.section .dic
word_HOLD:
	.word	word_EDIGS
	.byte	4
	.ascii	"HOLD"
HOLD:
	.word	code_ENTER
	.word	IMM,HOLDP
	.word	LOAD
	.word	IMM,1
	.word	SUB
	.word	DUP
	.word	IMM,HOLDP
	.word	STORE
	.word	CSTORE
	.word	RETURN
	
/*---------------------------------------------------------------------------*/
/* digit ( u -- c ) - convert digit u to a character.*/
	.section .dic
word_DIGIT:
	.word	word_HOLD
	.byte	5
	.ascii	"DIGIT"
DIGIT:
	.word	code_ENTER
	.word	IMM,9
	.word	OVER
	.word	LESS
	.word	IMM,7
	.word	AND
	.word	PLUS
	.word	IMM,'0'
	.word	PLUS
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* extract ( n base -- n c ) - extract the least significant digit from n. */
	.section .dic
word_EXTRACT:
	.word	word_DIGIT
	.byte	7
	.ascii	"EXTRACT"
EXTRACT:
	.word	code_ENTER
	.word	IMM,0
	.word	SWAP
	.word	UMMOD
	.word	SWAP
	.word	DIGIT
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* # ( u -- u ) - extract one digit from u and append the digit to output string. */
	.section .dic
word_DIG:
	.word	word_EXTRACT
	.byte	1
	.ascii	"#"
DIG:
	.word	code_ENTER
	.word	BASE
	.word	LOAD
	.word	EXTRACT
	.word	HOLD
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* #s ( u -- 0 ) - convert u until all digits are added to the output string. */
	.section .dic
word_DIGS:
	.word	word_DIG
	.byte	2
	.ascii	"#S"
DIGS:
	.word	code_ENTER
digs1:
	.word	DIG
	.word	DUP
	.word	BRANCHZ,digs2
	.word	BRANCH,digs1
digs2:
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* sign ( n -- ) - add a minus sign to the numeric output string. */
	.section .dic
word_SIGN:
	.word	word_DIGS
	.byte	4
	.ascii	"SIGN"
SIGN:
	.word	code_ENTER
	.word	ZLESS
	.word	BRANCHZ,sign1
	.word	IMM,'-'
	.word	HOLD
sign1:
	.word	RETURN


/*---------------------------------------------------------------------------*/
/*   str       ( n -- b u ) - convert a signed integer to a numeric string. */
	.section .dic
word_STR:
	.word	word_SIGN
	.byte	3
	.ascii	"str"
STR:
	.word	code_ENTER
	.word	DUP		/* n n */
	.word	TOR		/* n | R: n */
	.word	ABS		/* absn | R:n */
	.word	BDIGS		/* absn | R:n */
	.word	DIGS
	.word	RFROM
	.word	SIGN
	.word	EDIGS
	.word	RETURN

/*---------------------------------------------------------------------------*/
/*   u.    ( u -- ) - display an unsigned integer in free format. */
	.section .dic
word_UDOT:
	.word	word_STR
	.byte	2
	.ascii	"U."
UDOT:
	.word	code_ENTER
	.word	SPACE
	.word	BDIGS
	.word	DIGS
	.word	EDIGS
	.word	TYPE
	.word	RETURN

/*---------------------------------------------------------------------------*/
/*   .     ( w -- ) display an integer in free format, preceeded by a space. */
	.section .dic
word_DOT:
	.word	word_UDOT
	.byte	1
	.ascii	"."
DOT:
	.word	code_ENTER
	.word	BASE
	.word	LOAD
	.word	IMM,10
	.word	XOR
	.word	BRANCHZ,dot1
	/* Not decimal: display unsigned */
	.word	UDOT
	.word	RETURN
dot1:
	/* Decimal: display signed */
	.word	SPACE
	.word	STR
	.word	TYPE
	.word	RETURN

/*===========================================================================*/
/* Numeric input */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
	.section .dic
word_BASE:
	.word	word_DOT
	.byte	4
	.ascii	"BASE"
BASE:
	.word	code_ENTER
	.word	IMM,BASEP
	.word	RETURN

/*---------------------------------------------------------------------------*/
	.section .dic
word_HEX:
	.word	word_BASE
	.byte	3
	.ascii	"HEX"
HEX:
	.word	code_ENTER
	.word	IMM,16
	.word	BASE
	.word	STORE
	.word	RETURN

/*---------------------------------------------------------------------------*/
	.section .dic
word_DECIMAL:
	.word	word_HEX
	.byte	7
	.ascii	"DECIMAL"
DECIMAL:
	.word	code_ENTER
	.word	IMM,10
	.word	BASE
	.word	STORE
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( c base -- u t ) convert ascii to digit with success flag*/
	.section .dic
word_DIGITQ:
	.word	word_DECIMAL
	.byte	6
	.ascii	"DIGIT?"
DIGITQ:
	.word	code_ENTER
	.word	TOR
	.word	IMM,'0'
	.word	SUB

	.word	IMM,9
	.word	OVER
	.word	LESS

	.word	BRANCHZ,dgtq1

	.word	IMM,7
	.word	SUB

	.word	DUP
	.word	IMM,10
	.word	LESS
	.word	OR

dgtq1:
	.word	DUP
	.word	RFROM
	.word	ULESS
	.word	RETURN

/*---------------------------------------------------------------------------*/
/*   number?   ( cstr -- n t | a f ) - convert a number counted string to integer. push a flag on tos. */
	.section .dic
word_NUMBERQ:
	.word	word_DIGITQ
	.byte	7
	.ascii	"NUMBER?"
NUMBERQ:
	.word	code_ENTER
	.word	BASE		/*cstr &base */
	.word	LOAD		/*cstr base*/
	.word	TOR		/*cstr | R:base*/
	.word	IMM,0		/*cstr 0 | R:base*/
	.word	OVER		/*cstr 0 cstr | R:base*/
	.word	COUNT		/*cstr 0 strptr strlen | R:base*/

	.word	OVER		/*cstr 0 strptr strlen strptr | R:base*/
	.word	CLOAD		/*cstr 0 strptr strlen strptr[0] | R:base*/
	.word	IMM,'$'		/*cstr 0 strptr strlen strptr[0] $ | R:base*/
	.word	EQUAL		/*cstr 0 strptr strlen (1 if strptr[0]==$) | R:base*/
	.word	BRANCHZ,numq1	/*cstr 0 strptr strlen | R: base, jump if first buffer char is not $*/

	/* Equal returns 0 for FALSE (not equal). here we deal with $123 hex strings. */
	.word	HEX		/*cstr 0 strptr strlen | R:base */
	.word	SWAP		/*cstr 0 strlen strptr | R:base*/
	.word	IMM,1		/*cstr 0 strlen strptr 1 | R:base*/
	.word	PLUS		/*cstr 0 strlen strptr+1 | R:base TODO optimize CHARP or INC*/
	.word	SWAP		/*cstr 0 strptr+1 strlen | R:base*/
	.word	IMM,1		/*cstr 0 strptr+1 strlen 1 | R:base*/
	.word	SUB		/*cstr 0 strptr+1 strlen-1 | R:base TODO optimize DEC*/

numq1:	/* Buffer doesnt start with a $ sign. Check for initial minus sign. */
	.word	OVER		/*cstr 0 strptr strlen strbuf | R: base*/
	.word	CLOAD		/*cstr 0 strptr strlen strchar | R:base*/
	.word	IMM,'-'		/*cstr 0 strptr strlen strchar '-' | R:base*/
	.word	EQUAL		/*cstr 0 strptr strlen strchar=='-' */
	.word	TOR		/*cstr 0 strptr strlen | R:base -1_if_negative*/

	.word	SWAP		/*cstr 0 strlen strbuf | R:base -1_if_negative*/
	.word	RLOAD		/*cstr 0 strlen strbuf -1_if_neg | R:base -1_if_negative*/
	.word	SUB		/*cstr 0 strlen strbuf+1 | R:base -1_if_negative*/
	.word	SWAP		/*cstr 0 strbuf+1 strlen | R:base -1_if_negative*/
	.word	RLOAD		/*cstr 0 strbuf+1 strlen -1_if_neg | R:base -1_if_negative*/
	.word	PLUS		/*cstr 0 strbuf+1 strlen-1 | R:base -1_if_negative*/
	.word	DUPNZ		/*cstr 0 strbuf+1 strlen-1 [strlen-1 if not zero] | R:base -1_if_negative*/
	.word	BRANCHZ,numq6	/*jump to end if new len is zero*/

	.word	IMM,1		/*cstr 0 strptr strlen 1 | R:base -1_if_negative*/
	.word	SUB		/*cstr 0 strptr strlen-1 (for JNZD) | R:base -1_if_negative*/
	.word	TOR		/*cstr 0 strptr | R:base -1_if_negative strlen-1*/

numq2:
	.word	DUP		/*cstr 0 strptr strptr | R:base -1_if_negative strlen-1*/
	.word	TOR		/*cstr 0 strptr | R:base -1_if_negative strlen-1 strptr*/
	.word	CLOAD		/*cstr 0 strchar | R:base -1_if_negative strlen-1 strptr*/
	.word	BASE		/*cstr 0 strchar &base | R:base -1_if_negative strlen-1 strptr */
	.word	LOAD		/*cstr 0 strchar base | R:base -1_if_negative strlen-1 strptr */
	.word	DIGITQ		/*cstr 0 digit flag | R:base -1_if_negative strlen-1 strptr*/
	.word	BRANCHZ,numq4	/*cstr 0 digit - if failed (false) goto numq4 | R:base -1_if_negative strlen-1 strptr*/

	.word	SWAP
	.word	BASE
	.word	LOAD
	.word	STAR
	.word	PLUS
	.word	RFROM
	.word	IMM,1
	.word	PLUS
	.word	JNZD,numq2

	.word	RLOAD
	.word	SWAP
	.word	DROP
	.word	BRANCHZ,numq3

	.word	NEGATE

numq3:
	.word	SWAP
	.word	BRANCH,numq5

numq4:	/*invalid digit 	  cstr 0 digit | R:base -1_if_negative strlen-1 strptr*/
	.word	RFROM		/*cstr 0 digit strptr | R:base -1_if_negative strlen-1*/
	.word	RFROM		/*cstr 0 digit strptr strlen-1 | R:base -1_if_negative*/
	.word	DDROP		/*cstr 0 digit | R:base -1_if_negative*/
	.word	DDROP		/*cstr | R:base -1_if_negative*/
	.word	IMM,0		/*cstr 0 | R:base -1_if_negative*/
numq5:
	.word	DUP		/*cstr 0 0 |R:base is_negative*/
numq6:	/* Process String End	  cstr 0 strbuf | R:base is_negative */
	.word	RFROM		/*cstr 0 strbuf is_negative | R:base*/
	.word	DDROP		/*cstr 0 | R:base*/
	.word	RFROM		/*cstr 0 base */
	.word	BASE		/*cstr 0 base &BASE*/
	.word	STORE		/*cstr 0 */
	.word	RETURN

/*===========================================================================*/
/* Memory management */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
/* Push the address of the next free byte ( -- a) */
word_HERE:
	.word	word_NUMBERQ
	.byte	4
	.ascii	"HERE"
HERE:
	.word	code_ENTER
	.word	IMM, HEREP		/* (HEREP=&HERE) */
	.word	LOAD		/* (HERE) */ 
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* (u -- ) Pop a word and save it HERE, then make HERE point to the next cell */
	.section .dic
word_COMMA:
	.word	word_HERE
	.byte	1
	.ascii	","
COMMA:
	.word	code_ENTER
	.word	HERE		/* (VALUE) (HERE) */
	.word	DUP		/* (VALUE) (HERE) (HERE) */
	.word	CELLP		/* (VALUE) (HERE) (HERE+2) */
	.word	IMM, HEREP	/* (VALUE) (HERE) (HERE+2) (HEREP=&HERE) */
	.word	STORE		/* (VALUE) (HERE) */
	.word	STORE		/* Empty */
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* (u -- ) Pop a word and the LSB char it HERE, then make HERE point to the next cell */
	.section .dic
word_CCOMMA:
	.word	word_COMMA
	.byte	2
	.ascii	"C,"
CCOMMA:
	.word	code_ENTER
	.word	HERE		/* (VALUE) (HERE) */
	.word	DUP		/* (VALUE) (HERE) (HERE) */
	.word	CHARP		/* (VALUE) (HERE) (HERE+1) */
	.word	IMM, HEREP	/* (VALUE) (HERE) (HERE+1) (HP=&HERE) */
	.word	STORE		/* (VALUE) (HERE) */
	.word	CSTORE		/* Empty */
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* (val adr -- ) add val to the contents of adr */
	.section .dic
word_PLUS_STORE:
	.word	word_CCOMMA
	.byte	2
	.ascii	"+!"
PLUS_STORE:
	.word	code_ENTER
	.word	SWAP		/* adr val */
	.word	OVER		/* adr val adr */
	.word	LOAD		/* adr val *adr */
	.word	PLUS		/* adr val+*adr */
	.word	SWAP		/* val+*adr adr */
	.word	STORE		/*empty*/
	.word	RETURN

/*===========================================================================*/
/* Terminal */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
	.section .dic
word_BS:
	.word	word_PLUS_STORE
	.byte	2
	.ascii	"BS"
BS:
	.word	code_ENTER
	.word	IMM,8
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( -- 32 ) */
	.section .dic
word_BL:
	.word	word_BS
	.byte	2
	.ascii	"BL"
BL:
	.word	code_ENTER
	.word	IMM, 32
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* Emit a blank char */
	.section .dic
word_SPACE:
	.word	word_BL
	.byte	5
	.ascii	"SPACE"
SPACE:
	.word	code_ENTER
	.word	BL
	.word	EMIT
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* Emit a carriage return */
	.section .dic
word_CR:
	.word	word_SPACE
	.byte	2
	.ascii	"CR"
CR:
	.word	code_ENTER
	.word	IMM, 13
	.word	EMIT
	.word	IMM, 10
	.word	EMIT
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( buf bufend ptr -- buf bufend ptr  )  if ptr == buf */
/* ( buf bufend ptr -- buf bufend ptr-1)  if ptr  > buf */
/* Do a backspace: if not a bufstart, remove char from buf, then back, space, back */
	.section .dic
word_BKSP:
	.word	word_CR
	.byte	4
	.ascii	"BKSP"
BKSP:
	.word	code_ENTER
	/* check beginning of buffer */
	.word	TOR		/* buf bufend R: ptr */
	.word	OVER		/* buf bufend buf R: ptr */
	.word	RFROM		/* buf bufend buf ptr */
	.word	SWAP		/* buf bufend ptr buf */
	.word	OVER		/* buf bufend ptr buf ptr */
	.word	XOR		/* buf bufend ptr (buf == ptr) */
	.word	BRANCHZ,bksp1	/* buf bufend ptr */

	/* Remove char from buf */
	.word	IMM, 1		/* buf bufend ptr 1 */
	.word	SUB		/* buf bufend (ptr-1) */
	
	/* Send chars to erase output */
	.word	BS,EMIT
	.word	BL,EMIT		/* should replace emit by vectorable echo */
	.word	BS,EMIT
bksp1:
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( buf bufend ptr c -- buf bufend (ptr+1) ) accumulate character in buffer - no bounds checking */

	.section .dic
word_TAP:
	.word	word_BKSP
	.byte	3
	.ascii	"TAP"
TAP:
	.word	code_ENTER
	.word	DUP	/* buf bufend ptr c c */
	.word	EMIT	/* buf bufend ptr c | shoud be vectored to allow disable echo */
	.word	OVER	/* buf bufend ptr c ptr */
	.word	CSTORE	/* buf bufend ptr */
	.word	IMM,1	/* buf bufend ptr 1 */
	.word	PLUS	/* buf bufend (ptr+1) */
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* (buf bufend ptr c -- buf bufend ptr) */
	.section .dic
word_TTAP: /* should be vectorable */
	.word	word_TAP
	.byte	4
	.ascii	"TTAP"
TTAP:
	.word	code_ENTER
	.word	DUP		/*buf bufend ptr c c*/
	.word	IMM,13		/*buf bufend ptr c c 13*/
	.word	XOR		/*buf bufend ptr c (c==13)*/
	.word	BRANCHZ, ktap2	/*buf bufend ptr c | manage end of buf*/
	.word	BS		/*buf bufend ptr c 8*/
	.word	XOR		/*buf bufend ptr (c==8)*/
	.word	BRANCHZ, ktap1	/*buf bufend ptr | manage backspace*/
	.word	BL		/*buf bufend ptr 32 | replace other non-printable by spaces */
	.word	TAP		/*buf bufend ptr*/
	.word	RETURN
ktap1:	.word	BKSP		/*buf bufend ptr*/
	.word	RETURN
ktap2:	.word	DROP		/*buf bufend ptr*/
	.word	SWAP		/*buf ptr bufend*/
	.word	DROP		/*buf ptr */
	.word	DUP		/*buf ptr ptr*/
	.word	RETURN


/*---------------------------------------------------------------------------*/
/* ( buf len -- buf count) Read up to len or EOL into buf.
   Return buf and char count */
	.section .dic
word_ACCEPT:
	.word	word_TTAP
	.byte	6
	.ascii	"ACCEPT"
ACCEPT:
	.word	code_ENTER
	.word	OVER		/*buf len buf*/
	.word	PLUS		/*buf bufend*/
	.word	OVER		/*buf bufend bufcur , setup start, end, cur*/
ACCEPT1:
	.word	DDUP		/*buf bufend bufcur bufend bufcur*/
	.word	XOR		/*buf bufend bufcur (bufend==bufcur)*/
	.word	BRANCHZ,ACCEPT4	/*buf bufend bufcur              if buf reached bufend, finish word*/
	.word	KEY		/*buf bufend bufcur key */
	.word	DUP		/*buf bufend bufcur key key */
	.word	BL		/*buf bufend bufcur key key 32*/
	.word	IMM,127		/*buf bufend bufcur key key 32 127*/
	.word	WITHIN		/*buf bufend bufcur key (key is printable?)*/
	.word	BRANCHZ,ACCEPT2	/*buf bufend bufcur key , if not printable do ttap and loop again */
	.word	TAP		/*buf bufend bufcur , print and save printable key*/
	.word	BRANCH,ACCEPT1	/*buf bufend bufcur , again */
ACCEPT2:
	.word	TTAP		/*buf bufend bufcur , manage non printable key */
	.word	BRANCH,ACCEPT1	/*buf bufend bufcur , again */
ACCEPT4:
	.word	DROP		/*buf bufend - bufend has been replaced by bufcur in TTAP*/
	.word	OVER		/*buf bufend buf*/
	.word	SUB		/*buf len */
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( buf len --) Emit len chars starting at buf. */
	.section .dic
word_TYPE:
	.word	word_ACCEPT
	.byte	4
	.ascii	"TYPE"
TYPE:
	.word	code_ENTER
	.word	TOR		/* buf | R: len */
	.word	BRANCH,type2
type1:
	.word	DUP		/* buf buf | R: len */
	.word	CLOAD		/* buf char | R: len */
	.word	EMIT		/* buf */
	.word	IMM,1		/* buf 1 */
	.word	PLUS		/* buf+1 */
type2:	.word	JNZD,type1	/* if @R (==len) > 0 then manage next char */
	.word	DROP		/* remove buf from stack */
	.word	RETURN

/*===========================================================================*/
/* Parsing */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
/* (buf buflen delim -- buf len deltabuf) skip spaces, find word that ends at delim*/
	.section .dic
word_LPARSE:
	.word	word_TYPE
	.byte	5
	.ascii	"parse"
LPARSE:
	.word	code_ENTER	/**/
	.word	IMM,pTEMP	/*buf buflen delim &TEMP */
	.word	STORE		/*buf buflen */

	.word	OVER		/*buf buflen bufinit */
	.word	TOR		/*buf buflen | R: bufinit */
	.word	DUP		/*buf buflen buflen | R: bufinit */
	.word	BRANCHZ,pars8	/*buf buflen | R:bufinit if(buflen==0) goto pars8 */

	/* Buflen not zero */
	.word	IMM,1		/*buf len 1 | R:bufinit*/
	.word	SUB		/*buf len-1 | R:bufinit*/
	.word	IMM,pTEMP	/*buf len-1 &TEMP | R:bufinit*/
	.word	LOAD		/*buf len-1 delim | R:bufinit*/
	.word	BL		/*buf len-1 delim blank | R:bufinit*/
	.word	EQUAL		/*buf len-1 (delim==blank) | R:bufinit could it be simple xor?*/
	.word	BRANCHZ, pars3  /*buf len-1 jump to pars3 if delim is blank, else (delim not blank): continue */
	.word	TOR		/*buf | R: bufinit len-1*/
pars1:
	/* skip leading blanks only */
	.word	BL		/*buf blank | R: bufinit len-1 */
	.word	OVER		/*buf blank buf | R: bufinit len-1*/
	.word	CLOAD		/*buf blank curchar | R: bufinit len-1 */
	.word	SUB		/*buf curchar-bl | R: bufinit len-1 */
	.word	ZLESS		/*buf (curchar<blank) | R: bufinit len-1*/
	.word	NOT		/*buf (curchar>=blank) | R: bufinit len-1*/
	.word	BRANCHZ,pars2   /*buf | R: bufinit len-1 */

	/*curchar is below blank ->not printable, try next */
	.word	IMM, 1		/*buf 1 | R: bufinit len-1 */
	.word	PLUS		/*buf+1->buf | R: bufinit len-1*/
	.word	JNZD,pars1	/*buf | R: bufinit len-2 and goto pars1 or buf | R: bufinit continue if len-1 is null*/
	/*all chars parsed */
	.word	RFROM		/*buf len-1 */
	.word	DROP		/*buf */
	.word	IMM, 0		/*buf 0*/
	.word	DUP		/*buf 0 0*/
	.word	RETURN		/* all delim */

pars2:	/*Curchar >=delim */
	.word	RFROM		/*buf len-1*/

pars3:	/*Initial situation, delim is blank*/
	.word	OVER		/*buf len-1 buf*/
	.word	SWAP		/*buf buf len-1*/
	.word	TOR		/*buf buf | R:len-1*/

pars4:	/* scan for delimiter, beginning of a for loop */
	.word	IMM,pTEMP	/*buf buf &TEMP */
	.word	LOAD		/*buf buf delim */
	.word	OVER		/*buf buf delim buf */
	.word	CLOAD		/*buf buf delim curchar */
	.word	SUB		/*buf buf (delim-curchar) */

	.word	IMM,pTEMP	/*buf buf (delim-curchar) &TEMP */
	.word	LOAD		/*buf buf (delim-curchar) delim */
	.word	BL		/*buf buf (delim-curchar) delim blank */
	.word	EQUAL		/*buf buf (delim-curchar) (delim==blank) */
	.word	BRANCHZ,pars5	/*buf buf (delim-curchar) if(delim==blank) goto pars5 */
	.word	ZLESS		/*buf buf (delim<curchar)*/

pars5:	/* delim is blank */
	.word	BRANCHZ,pars6	/*buf buf if(delim<curchar) then goto par6 */
	.word	IMM,1		/*buf buf 1*/
	.word	PLUS		/*buf (buf+1)*/
	.word	JNZD,pars4	/*buf (buf+1) and loop to pars4 if (len-1)>0*/
	.word	DUP		/*buf (buf+1) (buf+1)*/
	.word	TOR		/*buf (buf+1) | R:(buf+1)*/
	.word	BRANCH,pars7	

pars6:	/*delim<curchar*/
	.word	RFROM
	.word	DROP
	.word	DUP
	.word	IMM,1
	.word	PLUS
	.word	TOR

pars7:
	.word	OVER
	.word	SUB
	.word	RFROM
	.word	RFROM
	.word	SUB
	.word	RETURN

pars8:	/* Empty buffer case */	/*buf 0 | R:bufinit */
	.word	OVER		/*buf 0 buf | R:bufinit*/
	.word	RFROM		/*buf 0 buf bufinit*/
	.word	SUB		/*buf 0 0*/
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* (delim -- buf len) parse TIB at current pos and return delim spaced word */
	.section .dic
word_PARSE:
	.word	word_LPARSE
	.byte	5
	.ascii	"PARSE"
PARSE:
	.word	code_ENTER
	/* Compute current input buffer pointer */
	.word	TOR		/* -- | R: delim*/
	.word	IMM, TIB	/* tib */
	.word	IMM, INN	/* tib &done_count */
	.word	LOAD		/* tib done_count */
	.word	PLUS		/* buf */
	/* Compute remaining count */
	.word	IMM,NTIB	/* buf &ntib */
	.word	LOAD		/* buf ntib */
	.word	IMM, INN	/* buf ntib &done_count */
	.word	LOAD		/* buf ntib done_count */
	.word	SUB		/* buf remaining_count */
	.word	RFROM		/* buf remaining_count delim */
	/* Call low level word */
	.word	LPARSE		/* buf wordlen delta */
	.word	IMM, INN	/* buf wordlen delta &done_count */
	.word	PLUS_STORE	/* buf wordlen */
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* (Parse-word')' -- ) Display a string */
	.section .dic
word_DOTPAR:
	.word	word_PARSE
	.byte	2
	.ascii	".("
DOTPAR:
	.word	code_ENTER
	.word	IMM,')'
	.word	PARSE
	.word	TYPE
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* (Parse-word')' -- ) Inline comment, nop */
	.section .dic
word_PAR:
	.word	word_DOTPAR
	.byte	1
	.ascii	"("
PAR:
	.word	code_ENTER
	.word	IMM,')'
	.word	PARSE
	.word	DDROP
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* Line comment , discard the rest of the input buffer */
	.section .dic
word_BSLASH:
	.word	word_PAR
	.byte	1
	.ascii	"\\"
BSLASH:
	.word	code_ENTER
	.word	IMM, NTIB
	.word	LOAD
	.word	IMM, INN
	.word	STORE
	.word	RETURN

/*===========================================================================*/
/* Dic search */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
/* find ( cstr voc -- codeadr cstr | cstr f ) */
/* Search a name in a vocabulary (pointer to last entry of a chain). */
/* THIS WORD DEPENDS ON THE IMPLEMENTED DICT STRUCTURE */
	.section .dic
word_FIND:
	.word	word_BSLASH
	.byte	4
	.ascii	"FIND"
FIND:
	.word	code_ENTER
	/*compare cstr to current name stored at voc*/
;This version is used if the prev link is stored before the name
found1:
	.word	DUP		/*cstr voc voc */
	.word	LOAD		/*cstr voc prev */
	.word	TOR		/*cstr voc | R:prev */
	.word	CELLP		/*cstr nameptr | R:prev */
	
	.word	DDUP		/*cstr nameptr cstr nameptr | R:prev */
	.word	CCOMPARE	/*cstr nameptr equal_flag | R: prev*/
	.word	BRANCHZ,found	/*cstr nameptr jump if equal | R:prev */

	/* Strings are different */
	.word	DROP		/*cstr | R: prev*/
	.word	RFROM		/*cstr prev */
	.word	DUP		/*cstr prev prev*/
	.word	BRANCHZ,noprev	/*cstr prev, jmp if prev null*/

	/* previous is not null, look at prev word */
	.word	BRANCH,found1

	/* no previous word */
noprev:
	.word	DROP		/*cstr */
	.word	IMM,0		/*cstr false */
	.word	RETURN
found:
	.word	RFROM		/*cstr nameptr prev */
	.word	DROP		/*cstr nameptr */
	.word	DUP		/*cstr nameptr nameptr */
	.word	CLOAD		/*cstr nameptr namelen */
	.word	CHARP		/*cstr nameptr namelen+1 */
	.word	PLUS		/*cstr codeptr */
	.word	SWAP		/*codeptr cstr , as required by spec*/
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( cstr -- codeaddr nameaddr | cstr false ) NAME? */
/* Check ALL vocabularies for a matching word and return code and name address, else same cstr and zero*/

	.section .dic
word_ISNAME:
	.word	word_FIND
	.byte	5
	.ascii	"NAME?"
ISNAME:
	.word	code_ENTER
	.word	IMM,LASTP	/*cstr [pointer containing the address of the last word] */
	.word	LOAD		/*cstr voc[address of last word entry] */
	.word	FIND		/*code name [if found] || cstr 0 [if not found] */
	.word	RETURN

/*===========================================================================*/
/* Error handling */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
	.section .dic
word_HANDLER:
	.word	word_ISNAME
	.byte	7
	.ascii	"HANDLER"
HANDLER:
	.word	code_ENTER
	.word	IMM,HANDP
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( xt -- err#\0 ) setup frame to handle errors thrown while executing xt */
	.section .dic
word_CATCH:
	.word	word_HANDLER
	.byte	5
	.ascii	"CATCH"
CATCH:
	.word	code_ENTER
	/* save error frame */
	.word	SPLOAD
	.word	TOR
	.word	HANDLER
	.word	LOAD
	.word	TOR
	/* Execute */
	.word	RPLOAD
	.word	HANDLER
	.word	STORE
	.word	EXECUTE
	/* Restore error frame */
	.word	RFROM
	.word	HANDLER
	.word	STORE
	/* No error */
	.word	RFROM
	.word	DROP
	.word	IMM,0
	.word	RETURN

/*---------------------------------------------------------------------------*/
	.section .dic
word_THROW:
	.word	word_CATCH
	.byte	5
	.ascii	"THROW"
THROW:
	.word	code_ENTER
	.word	RETURN

/*---------------------------------------------------------------------------*/
	.section .dic
word_ABORT:
	.word	word_THROW
	.byte	5
	.ascii	"ABORT"
ABORT:
	.word	code_ENTER
	.word	RETURN

/*---------------------------------------------------------------------------*/
	.section .dic
word_ABORTS:
	.word	word_ABORT
	.byte	6
	.ascii	"abort\""
ABORTS:
	.word	code_ENTER
	.word	RETURN

/*===========================================================================*/
/* Interpreter */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
/* ( a -- ) */
	.section .dic
word_INTERPRET:
	.word	word_ABORTS
	.byte	9
	.ascii	"INTERPRET"
INTERPRET:
	.word	code_ENTER
	.word	ISNAME		/*code name || cstr false */
	.word	BRANCHZ,donum	/* if not name then jump */

	/*Name is found */
	.word	EXECUTE
	.word	RETURN

donum:	/* No word was found, attempt to parse as number, then push */
	.word	NUMBERQ
	.word	BRANCHZ,notfound	/*consume the OK flag and leaves the number on the stack for later use*/
	.word	RETURN
notfound:
	.word	DROP
	.word	RETURN
	#.word	THROW

/*---------------------------------------------------------------------------*/
/* Set the system state to interpretation */
	.section .dic
word_INTERP:
	.word	word_INTERPRET
	.byte	1
	.ascii	"["
INTERP:
	.word	code_ENTER
	.word	IMM, INTERPRET
	.word	IMM, BEHAP
	.word	STORE
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* WORD and TOKEN. Create a counted string at HERE, which
   is used as temp memory. HERE pointer is not modified so each parsed word
   is stored at the same address. If an executed or compiled word manipulates HERE,
   then it is no problem: the user data will overwrite the word that was parsed and
   the next word will be stored a bit farther. It does not matter since this buffer
   is only used to FIND the code pointer for this word, usually.*/
/*(delim -- cs)*/
	.section .dic
word_WORD:
	.word	word_INTERP
	.byte	4
	.ascii	"WORD"
WORD:
	.word	code_ENTER
	.word	PARSE		/*buf len*/
	.word	HERE		/*buf len dest*/
	.word	PACKS		/*dest*/
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( -- cs) */
	.section .dic
word_TOKEN:
	.word	word_WORD
	.byte	5
	.ascii	"TOKEN"
TOKEN:
	.word	code_ENTER
	.word	BL
	.word	WORD
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* PROMPT */
	.section .dic
word_PROMPT:
	.word	word_TOKEN
	.byte	6
	.ascii	"PROMPT"
PROMPT:
	.word	code_ENTER
	.word	IMMSTR
	.byte	4
	.ascii	"  ok"
	.word	COUNT
	.word	TYPE
	.word	CR
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( -- ) evaluate all words in input buffer */
	.section .dic
word_EVAL:
	.word	word_PROMPT
	.byte	4
	.ascii	"eval"
EVAL:
	.word	code_ENTER
eval1:
	.word	TOKEN		/* tokcstr */
	.word	DUP		/* tokcstr tokcstr */
	.word	CLOAD		/* tokcstr toklen input stream empty?*/
	.word	BRANCHZ, eval2	/* tokcstr Could not parse: finish execution of buffer */
	.word	IMM,BEHAP	/* tokstr behap */
	.word	LOADEXEC	/* Execute contents of behap as sub-word if not null */
	/*.word	QSTAC*/		/*TODO Check stack */
	.word	BRANCH, eval1	/* Do next token */
eval2:
	.word	DROP		/*--*/
	.word	PROMPT		/**/
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( -- ) accept input stream to terminal input buffer. */
	.section .dic
word_QUERY:
	.word	word_EVAL
	.byte	5
	.ascii	"QUERY"
QUERY:
	.word	code_ENTER
	.word	IMM, TIB
	.word	IMM, TIB_LEN
	.word	ACCEPT

	#.word	CR,DDUP,TYPE,CR /* begin debug */

	/* Save the length of the received buffer */
	.word	IMM, NTIB
	.word	STORE
	.word	DROP

	.word	RETURN

/*---------------------------------------------------------------------------*/
/* Main forth interactive interpreter loop */
	.section .dic
word_QUIT:
	.word	word_QUERY
	.byte	4
	.ascii	"QUIT"
QUIT:
	.word	code_ENTER
QUIT0:
	.word	INTERP	/* Force interpretation mode in case catch aborts during compilation */

QUIT1:
	/* Load the terminal input buffer */
	.word	QUERY

	/* Reset input buffer pointer to start of buffer */
	.word	IMM,0
	.word	IMM,INN
	.word	STORE

	/* Eecute the line */
	.word	IMM,EVAL
	.word	CATCH		/* returns zero if no error */
	.word	DUPNZ		/* Does nothing if no error */
	.word	BRANCHZ, QUIT1	/* Consume catch return code. If thats zero, no error, loop again */

	/* CATCH caught an error, DUPNZ left the error that was thrown on the stack */
	/* TODO CONSOLE reinstall console (setup IO vectors for console, in case IO was happening from another device) */
	.word	COUNT,TYPE	/* Display error message from throw */
	/* TODO PRESET reinit data stack to top */
	.word	BRANCH,QUIT0	/* Interpret again */

/*---------------------------------------------------------------------------*/
/* ( -- u ) */
	.section .dic
word_VER:
	.word	word_QUIT
	.byte	3
	.ascii	"VER"
VER:
	.word	code_ENTER
	.word	IMM, 0x100
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( -- ) */
	.section .dic
word_hi:
	.word	word_VER
	.byte	2
	.ascii	"hi"
hi:
	.word	code_ENTER
	.word	CR
	.word	IMMSTR
	.byte	18
	.ascii	"hc11 grxforth ver "
	.word	COUNT,TYPE
	.word	HEX,VER,BDIGS,DIG,DIG,IMM,'.',HOLD,DIG,EDIGS,TYPE
	.word	CR
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* Main forth interactive interpreter loop */
	.section .dic
word_BOOT:
	.word	word_hi
	.byte	4
	.ascii	"BOOT"
BOOT:
	.word	code_ENTER
BOOT1:
	/* Show a startup banner */
	.word	hi

	/* Setup environment */
	.word	DECIMAL

	.word	QUIT
