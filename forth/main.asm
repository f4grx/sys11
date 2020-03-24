/* Mini forth interpreter based on eForth with simplifications

   Data structures
   ===============

   Dictionary and Data space
   -------------------------

   The data space is used to store new word definitions and user data.
   It grows from low addresses to upper addresses in the external memory.

   Builtin words are stored in RODATA. They cant be changed, but can be
   overriden by user entries. Builtin entries do no have parameter fields.

   User definitions are allocated in the data space. The definitions are added
   one after the other, linking the new one to the previous one. The pointer to
   the last definition is maintained in LAST. The first user word points to the last
   entry in the builtin list. This allows overriding of any built-in word.

   Data is allocated in an incremental fashion starting from the lowest
   address. at any time, the HERE variable contains the address of the next
   free zone.

   Dictionary entry
   ----------------

   Each entry has the following structure:
   N bytes: the word itself, null terminated.
   1 byte:  flags (if necessary, this field will not be used initially)
   2 bytes: pointer to previous entry
   2 bytes: code pointer (ITC).
            ENTER:   execute a list of forth opcodes stored after this pointer.
            DOCONST: push the value of the stored constant
            DOVAR:   push the address of the named constant
            other:   native code implementation
   
   Parameter stack
   ---------------
   The parameter stack is used to store temporary data items. It shoud not grow
   to an extremely large value, so it size is fixed when the interpreter is
   initialized.
   The stack grows down, it starts at the end of the RAM, and the storage
   address decreases on PUSH and increases on POP.
   For the moment underflow and overflows are not detected.

   Return stack
   ------------
   The return stack is used to push return addresses when nesting word
   executions. It is also used to manage control flow structures. Since this
   space is used a lot by complex programs, it is expected that limiting its
   size might prevent large programs from executing. Its size is only limited
   by the growth of the data space. It is initialized just below the maximum
   span of the parameter stack, and it can grows down until the return stack
   pointer hits HERE, the next allocation address in the data space.

   Registers
   ---------
   The forth interpreter uses registers for its internal use.
   SP is used as parameter stack pointer.
   Y  is used as return stack pointer.
   X and D can be used for calculations.
   a data word (IP) is used as instruction pointer.
   This is inspired by eForth (https://github.com/tonypdmtr/eForth/blob/master/hc11e4th.asm)

   To pop from return stack, we increment Y by 2 with ldab #2 aby then load at 0,y
   To push to the return stack, we store at 0,Y then dey dey
   The data stack can use the hc11 push and pulls.

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
IP:	.word	0		/* Instruction pointer */

	/* Input text buffering */

BASE:	.word	0	/* Value of the base used for number parsing */
NTIB:	.word	0	/* Number of characters in current line */
TIB:	.space	TIB_LEN	/* Input buffer */
IN:	.word	0	/* Parse position in the input buffer */
HERE:	.word	0	/* Address of next free byte in dic/data space */
LAST:	.word	0	/* Pointer to the last defined word or name */

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
	psha			/* We can use this instead of next to push a result before ending a word */
	pshb
NEXT:
	ldx	*IP		/* Get the instruction pointer */
NEXT2:				/* We can call here if X already has the value for IP */
	inx			/* Increment IP to look at next word */
	inx
	stx	*IP		/* Save IP for next execution round */
	dex			/* Redecrement, because we need the original IP */
	dex
	ldx	0,X		/* Deref: This IP contains a code pointer */
	jmp	0,X		/* Call the code that must run now */

/*---------------------------------------------------------------------------*/
/* Starts execution of a compiled word. The current IP is pushed on the return stack, then we jump */
/* This function is always called by the execution of NEXT. */
code_ENTER:
	inx			/* X has the address of the ENTER function. */
	inx
	ldd	*IP
	dey			/* Pre-Decrement Y by 2 to push */
	dey
	std	0,Y		/* Push the next IP to be executed after return from this thread */
	bra	NEXT2		/* Manage text opcode address */

/*---------------------------------------------------------------------------*/
/* Exit ends the execution of a word. The previous IP is on the return stack, so we pull it */
RETURN:
	ldx	0,Y		/* Get previous value for IP from top of return stack */
	ldab	#2
	aby			/* Increment Y by 2 to pop */
	bra	NEXT2		

/*===========================================================================*/
/* Internal words */
/* These words have no header because they cannot be compiled by the user.
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
	ldd	0,X
	inx
	inx
	stx	*IP
	xgdx
	bra	NEXT2

/*---------------------------------------------------------------------------*/
/* Pull a value. If zero, load next word in IP */
	.section .rodata
QBRANCH:
	.word	code_QBRANCH
	.text
code_QBRANCH:
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
/* Execute the code whose address is on the stack */
	.section .dic
word_EXECUTE:
	.word 0
	.asciz "EXECUTE"
EXECUTE:
	.word	code_EXECUTE

	.text
code_EXECUTE:
	pulx
	jmp	0,X


/*---------------------------------------------------------------------------*/
	.section .dic
word_EMIT:
	.word	word_EXECUTE
	.asciz	"EMIT"
EMIT:
	.word	code_EMIT

	.text
code_EMIT:
	pulb
	pula
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

/*---------------------------------------------------------------------------*/
	.section .dic
word_STORE:
	.word	word_KEY
	.asciz	"!"
STORE:
	.word	code_STORE

	.text
code_STORE:
	pulx
	pulb
	pula
	std	0,X
	bra	NEXT

/*---------------------------------------------------------------------------*/
	.section .dic
word_LOAD:
	.word	word_STORE
	.asciz "@"
LOAD:
	.word	code_LOAD

	.text
code_LOAD:
	pulx
	ldd	0,X
	bra	PUSHD

/*---------------------------------------------------------------------------*/
	.section .dic
word_DUP:
	.word	word_LOAD
	.asciz "DUP"
DUP:
	.word	code_DUP

	.text
code_DUP:
	tsx			/* Get stack pointer in X */
	ldd	0,X		/* Load top of stack in D */
	bra	NEXT		/* This will push top of stack again */


/*---------------------------------------------------------------------------*/
	.section .dic
word_ACCEPT:
	.word	word_DUP
	.asciz "ACCEPT"
ACCEPT:
	.word	code_ENTER
	.word	RETURN

/*===========================================================================*/
/* Other forth words implemented in forth.
 * These words are pre-compiled lists, they are all executed by code_ENTER.
 * The following words can only be pointers to cells containing references to
 * other words. Direct pointers to cells containing code addresses are not
 * possible.
 */
/*===========================================================================*/

/* Main forth interactive interpreter loop */
	.section .dic
word_QUIT:
	.word	word_ACCEPT
	.asciz "QUIT"
QUIT:
	.word	code_ENTER
QUIT2:
	.word	LITTERAL, TIB
	.word	LITTERAL, TIB_LEN
	.word	ACCEPT
	.word	BRANCH, QUIT2
	.word	RETURN /* Unreached */

	.text
/*===========================================================================*/
	.globl _start
_start:

	/* Init serial port */

	ldaa	#0x30
	staa	BAUD
	ldaa	#0x0C
	staa	SCCR2

	/* Init default values */
	ldx	#10
	stx	*BASE

	ldx	#word_QUIT
	stx	*LAST

	/* Setup the runtime environment */

	lds	#0x8000		/* Parameter stack at end of RAM */
	ldy	#0x7C00		/* Return stack 1K before end of RAM */
	ldx	#QUIT		/* load pointer to startup code */
	bra	NEXT2		/* Start execution */

