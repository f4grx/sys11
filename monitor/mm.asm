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
	ldx	#0xffff
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
	/* TODO check we have at least 2 spare bytes to store the size of the created zone */
	/* this is an inline routine. sr3 = curblock, sr2 = req size
	 * sr1 = cur->next
	 * TODO inline this subroutine
	 */
	ldx	sr3	/* after this X contains adr */
	ldd	sr2	/* after this D contains size */
	std	0,X	/* POKE adr+SIZE, size */
	addd	sr3	/* after this D contains size+adr */
	addd	#2	/* after this D contains size+adr+2 = nxtadr */
	std	2,X	/* POKE adr+NEXT, nxtadr */
	xgdx		/* after this X contains nxtadr */
	ldd	sr1	/* after this D contains PEEK(adr+NEXT) */
	std	2,X	/* POKE nxtadr+NEXT, PEEK(adr+NEXT) */
	ldy	sr3	/* after this Y contains adr */
	ldd	0,Y	/* after this D contains PEEK(adr+SIZE) = freesize */
	subd	#2	/* after this D contains freesize-2 */
	subd	sr2	/* after this D contains freesize-2-size = new free size */
	std	0,X	/* POKE nxtadr+SIZE, PEEK(adr+size)-2-size */
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
.Lagain:
	ldx	sr3
	ldd	2,X	/* Load the pointer to next zone, preserve X */
	std	sr1	/* sr1 now contains pointer to next zone */
	ldx	0,X	/* Load the zone size */
	stx	sr0	/* sr0 now contains size of free zone */
	cpx	sr2	/* Compare with required size */
	blo	.Lnext	/* Current free block smaller than request? try next */
	/* At this point we have a zone that is big enough for allocation */
	beq	.Lalloc	/* Zone has the correct size */
	/* Zone is larger than requested, we have to do a split */	
	bsr	mm_split
	/* now the current free zone @sr3 has the correct size */

.Lalloc:
	/* Set the mem fields to allocate this zone */
	ldx	sr3		/* get the pointer to current zone */
	cpx	head		/* is cur zone (alloced) the head of list? */
	bne	.Lretblock	/* no: we can now return the block */
	/* yes: so the head is the zone after the allocated block */
	ldd	sr1 		/* Get next zone pointer */
	std	head		/* Update head */
.Lretblock:
	inx			/* Make X look at the usable data zone */
	inx
	xgdx			/* Store in retval */
	bra	.Lend		/* We're done! */

.Lnext:	/* Setup pointers to look at next block */
	ldx	sr1	/* Get curzone->next */
	cpx	#0xffff	/* Is next the end of the list? */
	beq	.Ldone	/* Yes, alloc was not possible, were done */
	stx	sr3	/* now current zone is next zone */
	bra	.Lagain	/* Try to fit the request in the next zone */
.Ldone:
	/* We reached the end of free zones without finding a big enough one */
	/* We have to fail the allocation by returning NULL */
	ldd	#0
.Lend:
	rts


/* Release a memory zone.
 * After this call the pointed memory zone is considered free for allocation.
 * Parameters:
 *   PUSH16 pointer_to_zone
 * Return value: None
 */
	.global mm_free
mm_free:
	/* Get size and zone pointer */
	/* Browse free list to find insertion point */
	/* Insert into free list */
	/* Coalesce */

/* Merge adjacent free zones in the heap
 * After this call, the free list does not contain any pair of contiguous free
 * zones. All free zones are maximally large.
 * Parameters: None
 * Return value: None
 */
mm_coalesce:
	rts

