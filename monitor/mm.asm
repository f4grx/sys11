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
	ldx	#RAMSTART
	ldd	#(RAMLEN-2)	/* Block size = all but two header bytes */
	std	0,X
	ldd	#0xffff
	std	2,X		/* This is the pointer to next (none) */
	stx	head		/* Initialize head of free zone list */
	rts

/* Allocate a memory zone.
 * Parameters:
 *   PUSH16 requested_size
 * Return value: Pointer to memory zone, possibly NULL if failed to alloc
 */
	.global mm_alloc
mm_alloc:
	/* Get requested size from stack into *sr2 */
	pulx
	stx	*sr2	/* *sr2 <- size */

	/* Browse all zones in the free list.
	 * We need one that is larger than the requested size
	 */
	ldx	head
	stx	*sr3	/* *sr3 <- adr */
.Lagain:
	ldx	*sr3	/* X <- adr */
	ldx	0,X	/* X <- cursiz */
	stx	*sr0	/* *sr0 <- cursiz */
	cpx	*sr2	/* Compare cursiz and size */
	blo	.Lnext	/* Current free block smaller than request? try next */
	/* At this point we have a zone that is big enough for allocation */
	beq	.Lalloc	/* Zone has the correct size */
	/* Zone is larger than requested, we have to do a split */	
.Lsplit:
	ldd	*sr2	/* D <- size */
	addd	#2	/* D <- size + 2 */
	addd	*sr3	/* D <- size + 2 + adr == nxtadr */
	std	*sr1	/* *sr1 <- nxtadr */

	ldx	*sr3	/* X <- adr */
	ldd	0,X	/* D <- PEEK(adr+SIZE) */
	subd	#2	/* D <- PEEK(adr+SIZE) - 2 */
	subd	*sr2	/* D <- PEEK(adr+SIZE) - 2 - size == tmp */
	ldx	*sr1	/* X <- nxtadr */
	std	0,X	/* POKE(nxtadr+SIZE, tmp)*/

	ldx	*sr3	/* X <- adr */
	ldd	2,X	/* D <- PEEK(adr+NEXT) == tmp */
	ldx	*sr1	/* X <- nxtadr */
	std	2,X	/* POKE(nxtadr+NEXT, tmp) */

	ldx	*sr3	/* X contain adr */
	ldd	*sr1	/* D <- nxtadr */
	std	2,X	/* POKE adr+NEXT, nxtadr */
	ldd	*sr2	/* D <- size */
	std	0,X	/* POKE adr+SIZE, size */

	/* now the current free zone @*sr3 has the correct size */

.Lalloc:
	/* Set the mem fields to allocate this zone */
	ldx	*sr3		/* X <- adr */
	cpx	head		/* is cur zone (alloced) the head of list? */
	bne	.Lretblock	/* no: we can now return the block */
	/* yes: so the head is the zone after the allocated block */
	ldd	*sr1 		/* Get next zone pointer */
	std	head		/* Update head */
.Lretblock:
	inx			/* X still has *sr3(adr), skip size */
	inx			/* skip size */
	xgdx			/* put in D, X gets garbage */
	bra	.Lend		/* We're done! */

.Lnext:	/* Setup pointers to look at next block */
	ldx	*sr1	/* Get curzone->next */
	cpx	#0xffff	/* Is next the end of the list? */
	beq	.Ldone	/* Yes, alloc was not possible, were done */
	stx	*sr3	/* now current zone is next zone */
	bra	.Lagain	/* Try to fit the request in the next zone */
.Ldone:
	/* We reached the end of free zones without finding a big enough one */
	/* We have to fail the allocation by returning NULL */
	clra	/* DL <- 0 */
	clrb	/* DH <- 0 */
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
	pulx		/* X <- pointer to data to free */
	dex		/* Get pointer to zone */
	dex		/* X <- adr */
	stx	*sr0	/* sr0 <- adr */
	cpx	head	/* Compare adr with head */
	bhs	.Lbrowse
	/* adr is < head, replace head */
	ldd	head	/* D <- head */
	std	2,X	/* POKE(adr+NEXT, head) */
	stx	head	/* head <- adr */
	bra	.Lcoalesce

	/* Browse free list to find insertion point */
.Lbrowse:
	ldx	head	/* X == cur <- head */
.Lbrowsenext:
	cpx	#0xFFFF	/* Check end of list */
	beq	.Leol
	ldd	2,X	/* D == nxt <- PEEK(cur+NEXT) */
	cpd	*sr0
	bhi	.Leol	/* break if next > adr, D has nxt, X has cur */
	xgdx		/* X == cur <-> D == nxt : cur = nxt*/
	bra	.Lbrowsenext
.Leol:
	/* Insert into free list */
	std	2,x	/* POKE(cur+NEXT, nxt) */
	xgdx		/* D <- cur */
	ldx	*sr0	/* X <- adr */
	std	2,x	/* POKE(adr+NEXT, cur) */

	/* Coalesce - merge block if current block finishes right before next block (cur+size+2==next) */
.Lcoalesce:
	ldx	head	/* X == cur <- head */
.Lcoalloop:
	cpx	#0xFFFF	/* end of list reached? */
	beq	.Lcoaldone
	stx	*sr2	/* sr2 <- cur (has to be saved for use later in block merge) */
	ldd	2,X
	std	*sr1	/*sr1 == D <- nxt == PEEK(cur+NEXT) */
	ldd	0,X
	std	*sr0	/*sr0 == D <- siz == PEEK(cur+SIZE) */
	inx
	inx		/* Double inc X saves one byte wrt addd #0x0002 */
	xgdx		/* D <- cur + 2 */
	addd	*sr0	/* D <- cur + 2 + siz */
	cpd	*sr1	/* Compare with next */
	bne	.Lcoalnext
.Lcoaldo:
	/* free blocks adr and nxt are adjacent, can be merged */
	ldx	*sr1	/* X <- nxt */
	ldd	2,X	/* D <- PEEK(nxt+SIZE) */
	addd	#2	/* D <- PEEK(nxt+SIZE) + 2 */
	addd	*sr0	/* D <- siz + nxtsiz + 2 */
	ldx	*sr2	/* X <- cur */
	std	0,X	/* POKE(cur+SIZE, D) */
	ldy	*sr1	/* Y <- nxt */
	ldd	2,Y	/* D == nxt = PEEK(nxt+NEXT) */
        std	2,X	/* POKE(cur+NEXT, nxt) */
	bra	.Lcoalesce	/* Try again */
.Lcoalnext:
	/* These blocks are not adjacent, replace cur by next and try again for next pair */
	ldx	*sr1
	bra	.Lcoalloop
.Lcoaldone:
	rts

