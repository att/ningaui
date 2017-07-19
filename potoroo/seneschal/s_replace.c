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

/* 
-----------------------------------------------------------------------------------------
  Mnemonic:	replace - replace target(s) in a string 
		freplace()
		vreplace()
		s_sendfile()
		wexpand() - expand woomera things

  Abstract: 	accepts a source string, target and value. replaces each target found
		within the source string with  the value string. target is a string that 
		exists zero or more times in the source string. 
  Parms:	src - pointer to null terminated string
		target - string to match (NOT a pattern)
		value - pointer to the null terminated string that will replace %x patterns
		
 Returns: 	pointer to new, expanded, string. User must free.
 Date:		10 July 2001
 Author: 	E. Scott Daniels
 Mod:		27 Mar 2003 (sd) - converted sendfile to s_sendfile to avoid bsd socket.h
			conflict.
		27 Oct 2003 (sd) - now uses $TMP for local tmp file creation
		04 Jan 2006 (sd) - we expect null values for replacement strings; do not 
			bleat unless we are really chatty.
-----------------------------------------------------------------------------------------
*/

#include	<unistd.h>
#include	<stdio.h>
#include	<string.h>
#include	<errno.h>
#include	<pthread.h>

#include	<sfio.h>
#include	<ningaui.h>
#include	<ng_basic.h>
#include	<ng_lib.h>
#include	<ng_ext.h>

#include <ningaui.h>

static void s_sendfile(char *nodename, char *fname, char **inf, int ninf, int base, int count);


char *replace( char *src, char *target, char *value )
{
	char	es[NG_BUFFER];		/* expanded string */
	char	*nt = NULL;		/* next target match in src */
	int	vlen = 0;		/* length of the value inserted */
	int	tlen = 0;		/* length of target string */
	int	eidx = 0;		/* insertion point in es */

	vlen = strlen( value );
	tlen = strlen( target );

	nt = strstr( src, target );
	while( nt && *src )
	{
		strncpy( es + eidx, src, nt - src );	/* save all up to the target */
		eidx += nt - src;
		strcpy( es + eidx, value );
		eidx += vlen;
		src += (nt - src) + tlen;
		if( *src )
			nt = strstr( src, target );
	}

	if( *src )
		strcpy( es + eidx, src );
	else
		es[eidx] = 0;

	return ng_strdup( es );
}

/* 
-----------------------------------------------------------------------------------------
  Mnemonic:	vreplace - replace target(s) in a string 
  Abstract: 	Targets are of the form sn where s is a string (e.g. %o) and 
		n is used as an index into the array of things (strings) that the user 
		has passed in. Thus if s was defined as %o then %o1 would match the first
		element of things. n is one based and is converted to be zero based 	
		when accessing things.
  Parms:	src - pointer to null terminated string
		target - the target string to look for.
		things - pointer to an array of string pointers %1 matches the 0th element
		nthings - The number of things in the array;
		
 Returns: 	pointer to new, expanded, string. User must free.
 Date:		10 July 2001
 Author: 	E. Scott Daniels
-----------------------------------------------------------------------------------------
*/
char *vreplace( char *src, char *target, char **things, int nthings )
{
	char	es[NG_BUFFER*10];		/* expanded string */
	char	*nt;			/* next target */
	char	*et;			/* end of the %nn target */
	int	idx = 0;			/* index into things */
	int	eidx = 0;			/* index into es */
	int	tlen;			/* len of target string */

	tlen = strlen( target );
	nt = strstr( src, target );
	while( *src && nt )
	{
		if( nt != src )
		{
			strncpy( es + eidx, src, nt - src );		/* save all up to target */
			eidx += nt - src;
		}

		for( et = nt+tlen; et && isdigit( *et ); et++ );	/* at first character past <target>nn */
		if( et - (nt+tlen) > 0 )				/* really a number there */
		{
			if( (idx = atoi( nt+tlen )) <= nthings && idx > 0 && things[idx-1] )	/* make idx 0 based from user 1 based */
			{
				idx--;
				strcpy( es + eidx, things[idx] );
				eidx += strlen( things[idx] );
			}
			else
			{
				if( verbose > 2 || idx > nthings )		/* we expect NULL things now, so yodle only if being chatty */
					ng_bleat( 0, "vreplace: target index %d out of range or NULL target", idx );
			}

			src = et;
		}
		else			/* %something but not %n */
		{
			es[eidx++] = *nt;
			src = nt+1;
		}	

		if( *src )
			nt = strstr( src, target );
	}

	if( *src )
		strcpy( es + eidx, src );		/* snag anything that was left */
	else
		*(es + eidx) = 0;			/* nothing left, be parinoid and ensure end of string */

	return ng_strdup( es );
}

/* 
-----------------------------------------------------------------------------------------
  Mnemonic:	freplace - replace %F target(s) in a string 
  Abstract: 	%F targets are of the form %Fk,%[io]n which convert to a filename
		whose contents are the actual k pathnames for variables  starting with 
		%in, and continuing %i(n+1), ..., %i(n+k-1). As with all %[io] references, 
		n is origin 1.
  Parms:	src - pointer to null terminated string
		target - the target string to look for.
		things - pointer to an array of string pointers %1 matches the 0th element
		nthings - The number of things in the array;
		
 Returns: 	pointer to new, expanded, string. User must free.
 Date:		9 sep 2002
 Author: 	Andrew Hume
-----------------------------------------------------------------------------------------
*/
char *
freplace(char *nodename, char *cmd, char **inf, int ninf, char **outf, int noutf)
{
	/*static char *heredoc = "/ningaui/tmp/sene_tmp%d"; */
	static char *heredoc = NULL;
	static int here_num = 0;
	char	*ltdname = NULL;			/* $TMP on this node */
	char output[NG_BUFFER];
	char fname[NG_BUFFER];
	int count, base;
	char *op; 				/* pointer into the output buffer */
	char *s;
	char *ocmd = cmd;
	int n;

	if( ! heredoc )
	{
		sfsprintf( output, sizeof( output ), "sene_tmp%%d", ltdname );		/* fmt string */
		heredoc = ng_strdup( output );
		ng_bleat( 2, "freplace: created fmt string: %s", heredoc );
	}

	if(here_num == 0)
		here_num = lrand48();
	op = output;
	while(s = strstr(cmd, "%F"))
	{
		if(n = s-cmd){
			memcpy(op, cmd, n);
			cmd += n;
			op += n;
		}

		/* get count */
		count = strtol(s+2, &cmd, 10);
		if(memcmp(cmd, ",%i", 3) == 0) 		/* input */
		{
			base = strtol(cmd+3, &cmd, 10);
			sfsprintf(fname, sizeof fname, heredoc, here_num++);
			s_sendfile(nodename, fname, inf, ninf, base, count);
		
			sfsprintf( op, sizeof( output ) - strlen( op ) -1, "$TMP/seneschal/%s ", fname );
			/*strcpy(op, fname); */
			op = strchr(op, 0); 
		} 
		else 
		if(memcmp(cmd, ",%o", 3) == 0) 	/* output */
		{
			base = strtol(cmd+3, &cmd, 10);
			sfsprintf(fname, sizeof fname, heredoc, here_num++);
			s_sendfile(nodename, fname, outf, noutf, base, count);
			sfsprintf( op, sizeof( output ) - strlen( op ) -1, "$TMP/seneschal/%s ", fname );
			/*strcpy(op, fname); */
			op = strchr(op, 0);
		} 
		else 
		{
			ng_bleat( 0, "freplace: missing ,%%[io] at %10.10s", cmd );
			strcpy(op, "/dev/null/null");
			op = strchr(op, 0);
			/*cmd = s; */
		}
	}

	strcpy(op, cmd);
	ng_bleat(3, "freplace: '%s' -> '%s'", ocmd, output);
	return ng_strdup(output);
}

/*
	open a temp file and write the list of files pointed to by inf[ninf] pointers.
	close the file and ccp it to the target node. 
	fname is expected to be the basename of the flist file to create
*/
static void
s_sendfile(char *nodename, char *fname, char **inf, int ninf, int base, int count)
{
	int	i;
	Sfio_t	*fp;
	char	cmd[NG_BUFFER]; 
	char	localf[NG_BUFFER];	/* local file name: $TMP/seneschal/stuff */
	char	rmtf[NG_BUFFER];	/* remote file name, just seneschal/stuff as -t option on ccp command converts remote TMP */
	static	char *ltdname = NULL;

	if( ! ltdname )
	{
		if( (ltdname = ng_env( "TMP" )) == NULL )			/* we should panic and drop a load if this happens */
		{
			ng_bleat( 0, "freplace; unable to determine TMP!" );
			return;
		}
	}
		
	sfsprintf( rmtf, sizeof( rmtf ), "seneschal/%s", fname );
	sfsprintf(localf, sizeof( localf ), "%s/seneschal/%s_", ltdname, fname);	/* create local file in TMP/seneschal */
	if((fp = ng_sfopen(0, localf, "w")) == NULL){
		ng_bleat(0, "couldn't create %s: %s", localf, ng_perrorstr());
		return;
	}
	for(i = 0; i < count; i++){
		ng_bleat(3, "s_sendfile(%s, %s): %s", nodename, localf, inf[base+i-1]);
		sfprintf(fp, "%s\n", inf[base+i-1]);
	}

	if( sfclose(fp) != 0 )
	{
		ng_bleat( 0, "write error sendfile aborted operation: %s: %s", localf, strerror( errno ) );
		return;
	}

      	sfsprintf(cmd, sizeof cmd, "ng_ccp -t -d %s %s %s; rm %s", rmtf, nodename, localf, localf);	/* copy, then remove the local */
/*	sfsprintf(cmd, sizeof cmd, "ng_ccp -t -d %s %s %s", rmtf, nodename, localf );*/
	ng_bleat(2, "s_sendfile: %s\n", cmd);
	system( cmd );
}

#ifdef SELF_TEST
main( int argc, char **argv )
{

	char	*src = argv[1];
	char	*things[15];
	int i;
	char *new;

	for( i = 2; i < argc; i ++ )
		things[i-2] = argv[i];

	new = vreplace( src, "%o", things, i-2 );
	printf( "new=(%s)\n", new );
	ng_free( new );

	new = replace( src, "%j", argv[0] );
	printf( "new=(%s)\n", new );
	ng_free( new );
}
#endif
