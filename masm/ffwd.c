/*
 *      file I/O version of forward ref handler
 */

#define	FILEMODE	0644	/* file creat mode */
#define	UPDATE		2	/* file open mode */
#define	ABS		0	/* absolute seek */

int	Forward =0;		/* temp file's file descriptor	*/
const char	Fwd_name[] = { "Fwd_refs" } ;

/*
 *      fwdinit --- initialize forward ref file
 */
fwdinit()
{
	Forward = creat(Fwd_name,FILEMODE);
	if(Forward <0)
		fatal("Can't create temp file");
	close(Forward); /* close and reopen for reads and writes */
	Forward = open(Fwd_name,UPDATE);
	if(Forward <0)
		fatal("Forward ref file has gone.");
#ifndef DEBUG
	unlink(Fwd_name);
#endif
}

/*
 *      fwdreinit --- reinitialize forward ref file
 */
fwdreinit()
{
	F_ref   = 0;
	Ffn     = 0;
	lseek(Forward,0L,ABS);   /* rewind forward refs */
	read(Forward,&Ffn,sizeof(Ffn));
	read(Forward,&F_ref,sizeof(F_ref)); /* read first forward ref into mem */
	if (Debug) printf("First fwd ref: %d,%d\n",Ffn,F_ref);
}

/*
 *      fwdmark --- mark current file/line as containing a forward ref
 */
fwdmark()
{
	write(Forward,&Cfn,sizeof(Cfn));
	write(Forward,&Line_num,sizeof(Line_num));
}

/*
 *      fwdnext --- get next forward ref
 */
fwdnext()
{
	int stat;

	stat = read(Forward,&Ffn,sizeof(Ffn));

	if (Debug) printf("Ffn stat=%d ",stat);

	stat = read(Forward,&F_ref,sizeof(F_ref));

	if (Debug) printf("F_ref stat=%d  ",stat);

	if( stat < 2 )
	{
		F_ref=0;
		Ffn=0;
	}

	if (Debug) printf("Next Fwd ref: %d,%d\n",Ffn,F_ref);
}

