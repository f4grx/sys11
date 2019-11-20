/* Memory management routines */


	.include "softregs.inc"

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
	ldx	#(RAMLEN-2)	/* Block size = all but two header bytes */
	stx	RAMSTART
	ldx	#0
	stx	RAMSTART+2	/* This is the pointer to next (none) */
	ldx	#RAMSTART
	stx	head		/* Initialize head of free zone list */

	rts

/* Split a free zone in two.
 * After this call the memory now has two linked free zones:
 * one with the requested size (to be allocated immediately)
 * one with the remaining free size.
 * Parameters:
 *   PUSH16 free_zone_pointer
 *   PUSH16 requested_size
 * Return value: None
 * Side effect: the free_zone_ponter now has requested size and
 * a new smaller free zone has been created after the current free zone.
 */
mm_split:
	/*this is an inline routine. sr3 = curblock, sr2 = req size*/
	ldx	sr3	/* Get pointer */
	
	rts

/* Allocate a memory zone.
 * Parameters:
 *   PUSH16 requested_size
 * Return value: Pointer to memory zone, possibly NULL if failed to alloc
 */
	.global mm_alloc
mm_alloc:
	/* Get requested size from stack into sr2 */
	pulx
	stx	sr2

	/* Browse all zones in the free list.
	 * We need one that is larger than the requested size
	 */
	ldx	head
	stx	sr3	/* sr3 : pointer to the current zone */
again:
	ldx	sr3
	ldy	2,X	/* Load the pointer to next zone, preserve X */
	sty	sr1	/* sr1 now contains pointer to next zone */
	ldx	0,X	/* Load the zone size */
	stx	sr0	/* sr0 now contains size of free zone */
	cpx	sr2	/* Compare with required size */
	blo	next	/* Current free block smaller than request? try next */
	/* At this point we have a zone that is big enough for allocation */
	beq	allocate /* Zone has the correct size */
	/* Zone is larger than requested, we have to do a split */	
	ldx	sr3
	pshx		/* push current zone */
	ldx	sr2
	pshx		/* push requested size */
	bsr	mm_split
	/* now the current free zone @sr3 has the correct size */

allocate:
	/* Set the mem fields to allocate this zone */
	ldx	sr3	/* get the pointer to current zone */
	cpx	head	/* is the current zone (allocated) the head of list? */
	beq	replace_head	/* yes: so the head is the zone after the allocated block */
	bra	retblock	/* we can now return the block */
replace_head:
	stx	head
retblock:
	inx		/* Make X look at the usable data zone */
	inx
	xgdx		/* Store X in D */
	bra	end	/* We're done! */

next:	/* Setup pointers to look at next block */
	ldx	sr1	/* Get curzone->next */
	cpx	#0x0000	/* Is next the end of the list? */
	beq	done	/* Yes, alloc was not possible, were done */
	stx	sr3	/* now current zone is next zone */
	bra	again	/* Try to fit the request in the next zone */
done:
	/* We reached the end of free zones without finding a big enough one */
	/* We have to fail the allocation by returning NULL */
	ldd	#0
end:
	rts

/* Merge adjacent free zones in the heap
 * After this call, the free list does not contain any pair of contiguous free
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

