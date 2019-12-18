/* sys11 shell */

	.section .edata
scmdbuf:
	.space	80 /* Storage for command line */
scmdlen:
	.byte	0

	.text

	.func	shell_main
	.global shell_main
shell_main:
	clra
	staa	scmdlen
	rts
	.endfunc

