/* forth interpreter */
/* Data structures

   Dictionary
   ----------

   There are two dictionaries:
   - built in
   - User definitions.

   Built-in entries are stored in RODATA. They cant be changed, but can be
   overriden by user entries. Built-in entries do no have parameter fields.

   User definitions are in RAM. The definitions are added one after
   the others, linking to the previous one. The pointer to the last definition
   is maintained. The first RAM entry points to the built in list.
   This allows overriding of any built-in word.

   Dictionary entry
   ----------------

   Each entry has the following structure:
   1 byte, cut in half: high nibble contains flags
                        low nibble contains word length
   N bytes: the word itself.
   2 bytes; pointer to previous entry
   2 bytes: code pointer (ITC). Could be a JMP instruction (DTC), but larger.
            ENTER:   execute a list of forth opcodes stored after this pointer.
            DOCONST: push the value of the stored constant
            DOVAR:   push the address of the named constant
            other:   native code implementation
   P bytes: parameter field.

  Compilation: Each word is recognized and replaced by the address of its
  definition.

   Data area
   ---------

   Parameter stack
   ---------------

   Return stack
   ------------

*/
	.include "softregs.inc"
	.include "serial.inc"
	.include "stdio.inc"

	.equ	IBUF_LEN, 80
	.equ	DATA_STACK_SIZE, 512

	.section .edata
mtemp:	.word	0	/* Temp for maths */
dsp:	.word	0	/* Data stack pointer */
ibuf:	.space	IBUF_LEN+1
dstack:	.space	DATA_STACK_SIZE

	.section .rodata
msg:	.asciz "Forth for 68hc11\r\n"

bi_DOT:
	.byte 1
	.ascii "."
	.word 0 /* no previous */
	.word code_DOT

builtins:
bi_PLUS:
	.byte 1
	.ascii "+"
	.word bi_DOT
	.word code_PLUS

	.text
code_DOT:
	ldx	dsp
	cpx	#dstack
	beq	err_under
	ldd	0,X
	dex
	dex
	std	sp0
	jsr	serial_putdec
	jmp	NEXT

code_PLUS:
	ldx	dsp
	cpx	#dstack+2
	beq	err_under
	ldd	0,X
	dex
	dex
	std	mtemp
	ldd	0,X
	addd	mtemp
	std	0,X
	jmp	NEXT

err_under:
	
ENTER:
DOCONST:
DOVAR:

NEXT:
	/* Find a word in the definitions and return its address */
findword:
	rts

	.func	app_main
	.global app_main
app_main:
	ldaa	#OPT_ECHO		/* Default to echo ON */
	staa	rlopts

        /* Display prompt */

	ldx	#msg
	stx	*sp0
	jsr	serial_puts
	jsr	serial_crlf

.Linterp:

        /* Read a line and interpret */

	ldx	#ibuf
	stx	*sp0
	ldx	#(IBUF_LEN)	/* scmdbuf has additional byte to store 80 chars + final zero */
	stx	*sp1
	jsr	readline
	jsr	serial_crlf	/* Skip current line */

	/* Interpret each word */

.if 0
	/* Test display for readline */
	ldx	#scmdbuf
	stx	*sp0
	jsr	serial_puts
	jsr	serial_crlf	/* Skip current line */
	bra	.Linterp
.endif
	rts
	.endfunc

