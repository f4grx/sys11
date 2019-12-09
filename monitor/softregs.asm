/* Soft registers to store function parameters.
 * If value has to be preserved across a function call,
 * save/restore them on the stack around the nested call.
 */

	.global	sp0
	.lcomm	sp0, 2

	.global	sp1
	.lcomm	sp1, 2

	.global	sp2
	.lcomm	sp2, 2

	.global	sp3
	.lcomm	sp3, 2

/* Soft registers to store temporaries.
 * Value assumed to be destroyed after a function call.
 * Save on stack as needed.
 */

	.global	st0
	.lcomm	st0, 2

	.global	st1
	.lcomm	st1, 2

	.global	st2
	.lcomm	st2, 2

	.global	st3
	.lcomm	st3, 2



