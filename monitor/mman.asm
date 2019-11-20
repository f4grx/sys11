/* Memory management routines */


	.equ	RAMSTART, 0x0100
	.equ	RAMLEN  , 0x8000 - 0x0100

	.data
head:	.word	0 /* Pointer to the first free zone */

	.text

/* Initialize the heap.
 * We define a single free space starting at address RAMSTART
 * Parameters: None
 * Return value: None
 */
	.global	mm_init
mm_init:
	ldx	#(RAMLEN -2)	/* Block size = all but two header bytes */
	stx	RAMSTART
	ldx	#0xFFFF		/* This is the pointer to next (none) */
	stx	RAMSTART+2
	ldx	#0		/* Initialize head of free zone list */
	stx	head
	rts

/* Split a free zone in two.
 * After this call the memory now has two linked free zones:
 * one with the requested size
 * one with the remaining free size.
 * Parameters:
 *   PUSH16 free_zone_pointer
 *   PUSH16 requested_size
 * Return value: None
 * Side effect: the free_zone_ponter now has requested size and
 * a new smaller free zone has been created after the current free zone.
 */
mm_split:
	rts

/* Allocate a memory zone.
 * Parameters:
 *   PUSH16 requested_size
 * Return value: Pointer to memory zone, possibly NULL if failed to alloc
 */
	.global mm_alloc
mm_alloc:
	ldd	#0
	rts

/* Merge adjacent free zones in the heap
 * After this call, the free zone does not contain any pair of contiguous free
 * zones. All free zones are maximally large.
 * Parameters: None
 * Return value: None
 */
mm_coalesce:
	rts

/* Release a memory zone.
 * After this call the pointed memory zone is considered free for allocation.
 * Parameters:
 *   PUSH16 pointer_to_zone
 * Return value: None
 */
	.global mm_free
mm_free:
	rts

