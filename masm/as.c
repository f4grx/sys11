/*
 * The main code for the motorola freeware assembler package.
 * Hacked profusely from it's humble DOS origins into a
 * somewhat unix compatible state. (GFC 9/94)
 * 
 */

char mapdn();
char *alloc();
/*
 *	as ---	cross assembler main program
 */
main(argc,argv)
int	argc;
char	**argv;
{
	char	**np;
	char	*i;
	FILE	*fopen();
	int	j = 0;

	if(argc < 2){
		printf("Usage: %s [file(s)] [- l b h s c d]\n",argv[j]);
		printf("\nVersion: March 17, 1996\n");
		printf("    defaults to Motorola S19 file output\n");
		printf("-h: Intel hex file output\n");
		printf("-b: 64K binary image file output\n");
		printf("-l: list file\n");
		printf("-s: Symbol table dump\n");
		printf("-c: cross reference output\n");
		printf("-d: debug mode\n\n");
		exit(1);
		}
	  Argv = argv;

	  while ((j<argc) && (*argv[j] != '-'))
	   j++;
	  N_files = j-1;
	 if (j < argc )
	  {
	  argv[j]++;
	  while (j<argc)
	   {
	   for (i = argv[j]; *i != 0; i++)
	     if ((*i <= 'Z') && (*i >= 'A'))
	       *i = *i + 32;

	   if (strcmp(argv[j],"b")==0)		/* Binary file output */
	     Binfil = 1;
	   else if (strcmp(argv[j],"h")==0)	/* Hex file output */
	     Hexfil = 1;
	   else if (strcmp(argv[j],"l")==0)	/* Listing file */
	     Lflag = 1;
	   else if (strcmp(argv[j],"s")==0)	/* Symbol table dump */
	     Sflag = 1;
	   else if (strcmp(argv[j],"d")==0)	/* Debug mode */
	     Debug = 1;
	   else if (strcmp(argv[j],"c")==0)	/* Cross ref output */
	     CREflag = 1;
#if 0
	   else if (strcmp(argv[j],"y")==0)	/* cycle count (doesn't work)*/
	     Cflag = 1;
#endif
	    j++;
	   }
	  }

	initialize();

	root = NULL;

	Cfn = 0;
	np = argv;
	Line_num = 0; /* reset line number */
	while( ++Cfn <= N_files )
		if((Fd = fopen(*++np,"r")) == NULL)
		{
			printf("as: can't open %s\n",*np);
			exit(1);
		}
		else
		{
			make_pass();
			fclose(Fd);
		}
	if( Err_count == 0 ){
		Pass++;
		re_init();
		Cfn = 0;
		np = argv;
		Line_num = 0;
		while( ++Cfn <= N_files)
			if((Fd = fopen(*++np,"r")) != NULL)
			    {
				make_pass();
				fclose(Fd);
			     }
			if (Sflag == 1)
			  {
			    printf ("\f");
			    stable (root);
			  }
			if (CREflag == 1)
			  {
			    printf ("\f");
			    cross (root);
			  }
			finish_outfile ();
		}
	exit(Err_count);
}

initialize()
{
	FILE	*fopen();
	int	i = 0;

	if (Debug) printf("Initializing\n");
#if 0
	Lflag	  = 0;
	Cflag	  = 0;
	Sflag	  = 0;
	CREflag   = 0;
#endif
	Err_count = 0;
	Pc	  = 0;
	Pass	  = 1;
	Ctotal	  = 0;
	N_page	  = 0;
	Line[MAXBUF-1] = NEWLINE;

/*
 * Build the name of the output file.
 */
	strcpy(Obj_name,Argv[1]); /* copy first file name into array */

	do
	{
	    if (Obj_name[i]=='.') Obj_name[i]=0;
	}
	while (Obj_name[i++] != 0);

/*
 * Append the appropriate suffix to the output file.
 */
	if (Hexfil)	strcat(Obj_name,".hex");
	else if (Binfil)strcat(Obj_name,".obj");
	else		strcat(Obj_name,".s19");

	printf ("output:  %s\n", Obj_name);

	if( (Objfil = fopen(Obj_name,"w")) == NULL)
		fatal("Can't create object file");

	fwdinit();	/* forward ref init */
	localinit();	/* target machine specific init. */
}

re_init()
{
	if (Debug) printf("Reinitializing\n");

	Pc	= 0;
	E_total = 0;
	P_total = 0;
	Ctotal	= 0;
	N_page	= 0;
	fwdreinit();
}

make_pass()
{
	char	*fgets();

	if (Debug)
	{
		printf("\n------");
		printf("\nPass %d",Pass);
		printf("\n------\n");
	}

	while( fgets(Line,MAXBUF-1,Fd) != (char *)NULL ){
		Line_num++;
		P_force = 0;	/* No force unless bytes emitted */
		N_page = 0;
		   if(parse_line())
			process();
		if(Pass == 2 && Lflag && !N_page)
			print_line();
		P_total = 0;	/* reset byte count */
		Cycles = 0;	/* and per instruction cycle count */
		}
	f_record();
}


/*
 *	parse_line --- split input line into label, op and operand
 */
parse_line()
{
	register char *ptrfrm = Line;
	register char *ptrto = Label;
	char	*skip_white();
	static char hold_lbl[80];
	static int cont_prev = 0;

	if( *ptrfrm == '*' || *ptrfrm == '\n' ||
	    *ptrfrm == ';' || *ptrfrm == '#')
		return(0);	/* a comment line */

	while( delim(*ptrfrm)== NO )
		*ptrto++ = *ptrfrm++;
	if(*--ptrto != ':')ptrto++;     /* allow trailing : */
	*ptrto = EOS;

	ptrfrm = skip_white(ptrfrm);

	ptrto = Op;
	while( delim(*ptrfrm) == NO)
		*ptrto++ = mapdn(*ptrfrm++);
	*ptrto = EOS;

	ptrfrm = skip_white(ptrfrm);

	ptrto = Operand;
	while( *ptrfrm != NEWLINE )
		*ptrto++ = *ptrfrm++;
	*ptrto = EOS;

/* Below added by GFC 8/30/94 */

	if (cont_prev)
	{
		/*printf ("\n***** Continuing a previous line.\n");*/
		cont_prev = 0;
		strcpy (Label, hold_lbl);
	}

	if (Op[0] == ';')
	{
		if (Label[0] == '\0')
		{
			return(0);	/* a comment line */
		}
		else	/* save this label for the next parse_line() */
		{
			/*printf ("\n***** Found Label with no ops\n");*/
			strcpy (hold_lbl, Label);
			cont_prev = 1;
			return(0);	/* a comment line */
		}
	}


	if (Debug)
	{
		printf("\n");
		printf("Label      %s\n",Label);
		printf("Op         %s\n",Op);
		printf("Operand    %s\n",Operand);
	}

	return(1);
}

/*
 *	process --- determine mnemonic class and act on it
 */
process()
{
	register struct oper *i;
	struct oper *mne_look();

	Old_pc = Pc;		/* setup `old' program counter */
	Optr = Operand; 	/* point to beginning of operand field */

	if(*Op==EOS){		/* no mnemonic */
		if(*Label != EOS)
			install(Label,Pc);
		}
	else if( (i = mne_look(Op))== NULL)
		error("Unrecognized Mnemonic");
	else if( i->class == PSEUDO )
		do_pseudo(i->opcode);
	else
	{
		if( *Label )install(Label,Pc);
		if(Cflag)Cycles = i->cycles;
		do_op(i->opcode,i->class);
		if(Cflag)Ctotal += Cycles;
	}
}
