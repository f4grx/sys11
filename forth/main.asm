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

  Compilation: Each word is recognized and replaced by the address of its code pointer.
  Note this is not the address of the definition but the code pointer itself.
  This means that the NEXT operation is just:
  - load the contents of the next cell
  - jump to its contents.
  The last 

   Data area
   ---------

   Parameter stack
   ---------------

   Return stack
   ------------

*/
	.equ	IRAM_START  , 0x0040	/* Internal RAM for interpreter vars */
	.equ	IRAM_END    , 0x00FF
	.equ	RAM_START   , 0x0100	/* External RAM for dic,data,stacks */
	.equ	RAM_END     , 0x7FFF
	.equ	RAMEXT_START, 0x8000	/* Optional RAM extension */
	.equ	RAMEXT_END  , 0xBFFF
	.equ	IO_START    , 0xC000	/* I/O space for external peripherals */
	.equ    IO_END      , 0xDFFF
	.equ	ROM_START   , 0xE000	/* ROM containing forth interpreter */
	.equ	ROM_END     , 0xFFBF
	.equ	VEC_START   , 0xFFC0	/* Interrupt vectors */
	.equ	VEC_END     , 0xFFFF

    .text
    .globl _start
_start:
	jmp	_start

