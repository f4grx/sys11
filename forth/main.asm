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
	ldx	#QUIT1		/* load pointer to startup code, skipping the native ENTER pointer! */
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
	/* This is called with the address of instruction being run (aka forth opcode) in D*/
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
	.asciz "EXECUTE"
EXECUTE:
	.word	code_EXECUTE

	.text
code_EXECUTE:
	pulx		/* Retrieve a word address from stack */
	bra	NEXT2	/* Launch it */


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
	pula	/* PREV contains data, A = MSB, discarded */
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
	ldd	0,X		/* Load top of stack in D */
	bra	PUSHD		/* This will push top of stack again */

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
	ldd	2,X		/* Load value before top of stack */
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
/* Eforth defines UM+ ( u v -- u+v cy ) and then : + UM+ DROP ; */
/* We do a native version for speed */
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
	addd	0,X
	pulx
	bra	PUSHD

/*---------------------------------------------------------------------------*/
/* ( u v -- u^v ) */
	.section .dic
word_XOR:
	.word	word_PLUS
	.asciz	"XOR"
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
	.asciz	"AND"
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
	.asciz	"OR"
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
	.word	word_IMMSTR
	.asciz "2DUP"
DDUP:
	.word	code_ENTER
	.word	OVER
	.word	OVER
	.word	RETURN

/*===========================================================================*/
/* Math and logical */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
	.section .dic
word_NOT:
	.word	word_DDUP
	.asciz	"NOT"
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
	.asciz	"NEGATE"
NEGATE:
	.word	code_ENTER
	.word	NOT
	.word	IMM, 1
	.word	PLUS
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( a b -- a-b ) */
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
	.word	IMM,2
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
	.word	IMM,1
	.word	PLUS
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( u v -- u<v ) unsigned compare of top two items. */

	.section .dic
word_ULESS:
	.word	word_CHARP
	.asciz	"U<"
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
/* ( u ul uh -- ul <= u < uh ) */
	.section .dic
word_WITHIN:
	.word	word_ULESS
	.asciz	"WITHIN"
WITHIN:
	.word	code_ENTER
	.word	OVER		/*u ul uh ul*/
	.word	SUB		/*u ul (uh-ul) */
	.word	TOR		/*u ul R: (uh-ul) */
	.word	SUB		/*(u-ul) R: (uh-ul) */
	.word	RFROM		/* (u-ul) (uh-ul) */
	.word	ULESS		/* ((u-ul) < (uh-ul)) */
	.word	RETURN

/*===========================================================================*/
/* Strings */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
/* COUNT ( cstradr -- bufadr len ) Return the buf addr and len of a pointed counted string */
	.section .dic
word_COUNT:
	.word	word_WITHIN
	.asciz	"COUNT"
COUNT:
	.word	code_ENTER
	.word	DUP		/* cstradr cstradr */
	.word	IMM,1		/* cstradr cstradr 1*/
	.word	PLUS		/* cstradr bufadr */
	.word	SWAP		/* bufadr cstradr */
	.word	CLOAD		/* bufadr len */
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* IMMSTR ( -- adr ) Push the address of an inline counted string that follows this word */
	.section .dic
word_IMMSTR:
	.word	word_COUNT
	.asciz "IMMSTR"
IMMSTR:
	.word	code_ENTER
	.word	RFROM		/* adr of next word -> points to length of inline counted string */
	.word	DUP		/* cstradr cstradr*/
	.word	COUNT		/* cstradr bufadr len */
	.word	PLUS		/* cstradr endadr */
	.word	TOR		/* cstradr R: nextwordadr */
	.word	RETURN


/*===========================================================================*/
/* Memory management */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
/* Push the address of the next free byte ( -- a) */
word_HERE:
	.word	word_IMMSTR
	.asciz	"HERE"
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
	.asciz	","
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
	.asciz	"C,"
CCOMMA:
	.word	code_ENTER
	.word	HERE		/* (VALUE) (HERE) */
	.word	DUP		/* (VALUE) (HERE) (HERE) */
	.word	CHARP		/* (VALUE) (HERE) (HERE+1) */
	.word	IMM, HEREP	/* (VALUE) (HERE) (HERE+1) (HP=&HERE) */
	.word	STORE		/* (VALUE) (HERE) */
	.word	CSTORE		/* Empty */
	.word	RETURN

/*===========================================================================*/
/* Terminal */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
/* ( buf bufend ptr c -- buf bufend (ptr+1) ) accumulate character in buffer - no bounds checking */

	.section .dic
word_TAP:
	.word	word_CCOMMA
	.asciz	"TAP"
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
/* ( -- 32 ) */
	.section .dic
word_BL:
	.word	word_TAP
	.asciz	"BL"
BL:
	.word	code_ENTER
	.word	IMM, 32
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* Emit a carriage return */
	.section .dic
word_SPACE:
	.word	word_BL
	.asciz	"SPACE"
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
	.asciz	"CR"
CR:
	.word	code_ENTER
	.word	IMM, 13
	.word	EMIT
	.word	IMM, 10
	.word	EMIT
	.word	RETURN

/*---------------------------------------------------------------------------*/
	.section .dic
word_BS:
	.word	word_CR
	.asciz	"BS"
BS:
	.word	code_ENTER
	.word	IMM,8
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* ( buf bufend ptr -- buf bufend ptr  )  if ptr == buf */
/* ( buf bufend ptr -- buf bufend ptr-1)  if ptr  > buf */
/* Do a backspace: if not a bufstart, remove char from buf, then back, space, back */
	.section .dic
word_BKSP:
	.word	word_BS
	.asciz	"BKSP"
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
/* (buf bufend ptr c -- buf bufend ptr) */
	.section .dic
word_TTAP: /* should be vectorable */
	.word	word_BKSP
	.asciz	"TTAP"
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
/* ( buf len -- buf count) Read up to TIB_LEN or EOL into the provided buffer.
   Return buf and char count */
	.section .dic
word_ACCEPT:
	.word	word_TTAP
	.asciz "ACCEPT"
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
	.asciz "TYPE"
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
/* Compiler */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
/* Set the system state to interpretation */
	.section .dic
word_INTERP:
	.word	word_TYPE
	.asciz	"["
INTERP:
	.word	code_ENTER
	.word	IMM, EXECUTE
	.word	IMM, BEHAP
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
	.word	IMM, COMPILE
	.word	IMM, BEHAP
	.word	STORE
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* (u -- ) store u, actually similar to COMMA */
	.section .dic
word_COMPILE:
	.word	word_STARTCOMP
	.asciz	"COMPILE,"
COMPILE:
	.word	code_ENTER
	.word	COMMA
	.word	RETURN

/*---------------------------------------------------------------------------*/
/* Main forth interactive interpreter loop */
	.section .dic
word_QUIT:
	.word	word_COMPILE
	.asciz "QUIT"
QUIT:
	.word	code_ENTER
QUIT1:
	/* Show a startup banner */
	.word	CR
	.word	IMMSTR
	.byte	10
	.ascii	"hc11 forth"
	.word	COUNT
	.word	TYPE
	.word	CR

QUIT2:
	/* Load the terminal input buffer */
	.word	IMM, TIB
	.word	IMM, NTIB
	.word	LOAD
	.word	ACCEPT
	/* New line, then echo */
	.word	CR
	.word	TYPE
	.word	CR

	.word	BRANCH, QUIT2
	.word	RETURN /* Unreached */


