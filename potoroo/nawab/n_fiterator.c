/*
* ======================================================================== v1.0/0a157 
*                                                         
*       This software is part of the AT&T Ningaui distribution 
*	
*       Copyright (c) 2001-2009 by AT&T Intellectual Property. All rights reserved
*	AT&T and the AT&T logo are trademarks of AT&T Intellectual Property.
*	
*       Ningaui software is licensed under the Common Public
*       License, Version 1.0  by AT&T Intellectual Property.
*                                                                      
*       A copy of the License is available at    
*       http://www.opensource.org/licenses/cpl1.0.txt             
*                                                                      
*       Information and Software Systems Research               
*       AT&T Labs 
*       Florham Park, NJ                            
*	
*	Send questions, comments via email to ningaui_support@research.att.com.
*	
*                                                                      
* ======================================================================== 0a229
*/

/* included in n_jobs eventually */
/*
  -----------------------------------------------------------------------------------------
	make jobs using filenames from the file spceified in the iteration (apl)
	This is called when [ %f in <file(/path/fname)>] is placed on the job
	We will set %f1...n for each filename that exists on the input record
	so the user can code something like:
		[ %f in < contentsf(/path/junk) > ]
		<- a = %f1
		-> b = %f2

		[ %f in < contentsp("command|command") > ]
		<- a = %f1
		-> b = %f2

	and have two files per record in the junk file. As with all file references
	(e.g. %in and %on) numbering is one based.

	%f and %f1 are the same allowing user to code ->a = %f
	Right now the file must be local to the node running nawab.

	ivname is the iterator variable name (eg %f)
  -----------------------------------------------------------------------------------------
	Mods:
		28 Mar 2006 (sd) : now really frees the variables as it watches for the 
			max number of tokens seen, and not bugged by having ntoks set to 
			zero on the last read. 
		31 Oct 2006 (sd) : Fixed small memory leak (target not freed)
			converted to use ng_sfopen() and check sfclose() return.
		20 Mar 2007 (sd) : Fixed arg to var expansion function so that a new buffer
			is returned (freeing the static buffer started to cause issues under
			gccv4).
  -----------------------------------------------------------------------------------------
*/
int make_file_jobs( Programme *p, Troupe *t, Iter *it, char *ivname  )
{
	extern	int	yyerrors;
	extern	char	*yyerrmsg;
	Desc	*d;			/* associated descriptor */
	Job	*jp;
	Sfio_t	*f;			/* file with the list of files */

	char	ebuf[NG_BUFFER];
	int	i;
	int	jidx = 0;		/* job index */
	int	nalloc = 0;		/* number of job slots we have allocated -- variable based on file length */
	int	ntoks;			/* number of tokens read from the file */
	int	max_toks = 0;		/* max tokens we saw and created %xn vars for */
	char	**toks;			/* pointer to tokens read from the file (file names) */
	char	*tok;			/* token to do something with at some time */
	char	varname[10];		/* variable names for sym tab %f0 %f1... */
	char	*eop;			/* pointer to the end of the path string in the file name */

	char	*target	= NULL;		/* target filename/command that has/generates stuff to iterate over -- after expansion */

	d = t->desc;		/* shortcut */

	if( ! it->app )
	{
		yyerrors++;
		sfsprintf( ebuf, sizeof( ebuf ),  "make_file_jobs: unable to determine pipe/file: %s(troupe) %s(prog)",  d->name, p->name );
		if( ! yyerrmsg )
			yyerrmsg = strdup( ebuf );
		ng_bleat( 0, "%s", yyerrmsg );
		return 0;
	}

	target = ng_var_exp( symtab, p->progid + VAR_SPACE, it->app, '%', '^', ':', VEX_NEWBUF );	/* use VEX_CONSTBUF if constant buf needed */

	ng_bleat( 2, "make_file_jobs: after var_exp target  = (%s)", target );
	if( it->type == T_ifile )			/* open regular file */
		f = ng_sfopen(  NULL, target, "r");
	else
		f = sfpopen(  NULL, target, "r");	/* else assume pipeline generates stuff to iterate */
	if( ! f )
	{
		yyerrors++;
		sfsprintf( ebuf, sizeof( ebuf ),  "make_file_jobs: unable to open pipe/file: %s %s(troupe) %s(prog)", target, d->name, p->name );
		if( ! yyerrmsg )
			yyerrmsg = strdup( ebuf );
		sbleat( 0, "%s", yyerrmsg );
		ng_free( target );
		return 0;
	}
			

	t->njobs = 0;			/* we will count as we read from the file */

	if( d->nodes && (tok = strchr( d->nodes, ',' )) != NULL )
	{
		*tok = 0;			/* allow only one node here */
		sbleat( 0, "make_file_jobs: WARNING: nodes was a list for a file/pipe iteration, used first: %s", d->nodes );
	}

	toks = NULL;				/* initially no token pointer */
	while( (toks = ng_readtokens( f, ' ', '\\', toks, &ntoks )) != NULL )		/* read next set of filenames from the file */
	{
		if( ntoks > max_toks )
			max_toks = ntoks; 

		ng_bleat( 3, "make_fiter: read record from file; tok[0] of %d = %s", ntoks, toks[0] );
		if( jidx >= nalloc )			/* need more jobs */
		{
			nalloc += 100;
			t->jobs = ng_realloc( t->jobs, nalloc * sizeof( Job ), "make_file_jobs job alloc" );
			memset( &t->jobs[jidx], 0, sizeof( Job ) * 100 );
			ng_bleat( 3, "make_fiter: needed job alloc; nalloc now %d", nalloc );
		}

		sfsprintf( varname, sizeof( varname ), "%s", ivname);		/* %f maps to the same as %f0 */
		symlook( symtab, varname, p->progid + VAR_SPACE, toks[0], SYM_COPYNM );
		for( i = 0; i < ntoks; i++ )			/* set up %f variables for job making */
		{
			eop = strrchr( toks[i], '/' );
			if( eop )
				eop++;			/* point past path if there */
			else
				eop = toks[i];
			sfsprintf( varname, sizeof( varname ), "%s%d", ivname, i+1 );		/* %f1 %f2... */
			symlook( symtab, varname, p->progid + VAR_SPACE, eop, SYM_COPYNM );
			ng_bleat( 3, "make_fiter: adding symbol based on file info: i=%d v=%s f=%s(%s)", i, varname, toks[i], eop );
		}

		jp = &t->jobs[jidx];				/* makes it easier I think */
		make_s_job( p, jp, d, jidx, d->nodes );		/* build the seneschal job converting %f[n] into things */
		jp->node[0] = 0;				/* curious about this -- stolen from another make_ function */

		jidx++;			/* and on to the next one */
		t->njobs++;
	}

	sfsprintf( varname, sizeof( varname ), "%s", ivname);	/* %f maps to the same as %f0 */
	symdel( symtab, varname, p->progid + VAR_SPACE );
	for( i = 0; i < max_toks + 1; i++ )		/* set up %f variables for job making */
	{
		sfsprintf( varname, sizeof( varname ), "%s%d", ivname, i );	/* %f0 %f1... */
		ng_bleat( 3, "make_fiter: deleting symbol based on file info: i=%d ", i, varname );
		symdel( symtab, varname, p->progid + VAR_SPACE );
	}

	if( sfclose( f ) != 0 )
	{
		sbleat( 0, "ERROR: close reported error while reading from input file or pipe: %target", target );
		ng_free( target );
		return 0;		/* some input error */
	}

	ng_free( target );
	return 1;
}
