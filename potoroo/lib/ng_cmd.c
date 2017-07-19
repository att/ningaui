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
  Mnemonic:	ng_cmd
  Abstract:	ng_cmd: Run a command and return output in a buffer
		ng_bquote: expand first set of backquoted things in the buffer
 		
  Date:		30 Jan 2004 -- converted from nawab
  Author:	Andrew Hume
  ---------------------------------------------------------------------------------
*/
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<fcntl.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<string.h>
#include 	<signal.h>
#include	<errno.h>

#include	<sfio.h>

#include	"ningaui.h"
#include	"ng_ext.h"
#include	"ng_lib.h"

char *ng_cmd( char *cmd )
{
	char *ret;
	int aret, nret, k;
	Sfio_t *fp;
	static char *errval = "bquote cmd failed";

	if((fp = sfpopen(0, cmd, "r")) == NULL){
		sfprintf(sfstderr, "%s: `%s` failed: %s\n", argv0, cmd, ng_perrorstr());
		return errval;
	}
	aret = NG_BUFFER;
	ret = ng_malloc(aret, "ng_cmd alloc");
	nret = 0;
	while((k = sfread(fp, ret+nret, aret-nret)) > 0){
		nret += k;
		if(nret >= aret){
			aret = 1.5*aret;
			ret = ng_realloc(ret, aret, "bquote realloc");
		}
	}
	sfclose(fp);
	ret[nret] = 0;
	return ret;
}

/*	expand the first set of back quoted things.  
	no mechanism to escape bk quotes ( e.g. ` ...\`....\`....` not supported)
*/
char *ng_bquote( char *buf )
{
	char *rb = NULL;		/* return buffer */
	char *b1;			/* first back quote */
	char *b2;			/* last back quote */
	char *eb;			/* buffer back from ng_cmd */
	int l;				/* len of new buffer after expansion */

	if( (b1 = strchr( buf, '`' )) )	
	{
		if( (b2 = strchr( b1+1, '`' )) )
		{
			*b2 = 0;
			eb = ng_cmd( b1+1 );		/* run the command */
			*b2 = '`';			
			*b1 = 0;
			l = strlen( eb );
			if( *(eb+l-1) == '\n' )
			{
				*(eb+l-1) = 0;
				l--;
			}
			l += strlen( buf ) + strlen( b2+1 ) + 1;			
			rb = ng_malloc( l, "bquote buffer" );
			sfsprintf( rb, l, "%s%s%s", buf, eb, b2+1 );
			*b1 = '`';
			ng_free( eb );
		}
		else
			ng_bleat( 0, "bquote: closing back quote not found: %s", buf );
	}

	return rb;
}


#ifdef SELF_TEST
main( int argc, char **argv )
{
	char *b;

	if( strchr( argv[1], '`' ) )
		b = ng_bquote( argv[1] );
	else
		b = ng_cmd( argv[1] );
	printf( "%s\n", b ? b: "null string" );
}
#endif

/* ---------- SCD: Self Contained Documentation ------------------- 
.** there is an uneven number of back quotes above, stick one here because 
.** they are significant to xfm and when skipping to scd_start they seem 
.** to still be interpreted by something.
.** ----------------------------------------------------------------------
`
.** ----------------------------------------------------------------------
&scd_start
&doc_title(ng_cmd:Execute a command and return stdout in a single buffer)


&space
	#include <ningaui.h>
&break	#include <ng_lib.h>
&synop	char *ng_bquote(  char *buf )
&break
	char *ng_cmd( char *cmd )

&space
&desc	Ng_bquote will expand the first set of back-quoted tokens in buf.
	The expansion will be merged in with the other contents of 
	buf and returned as a new buffer to the caller. NULL is returned 
	if there are no backquotes in the buffer to expand, the caller's
	buffer is left unchanged, though it is temporarly modified during
	the processing. 
&space
	Ng_cmd assumes that &ital(buf) contains a completed command and will 
	execute the command. The result is a NULL terminated buffer which 
	contains the standard output of the command. 
	
&space
&subcat Bugs
	Ng_bquote does not provide for escaped back quotes in the buffer.
&space
&mods
&owner(Scott Daniels)
&lgterms
&term	30 Jan 2004 (sd) : Broken out of nawab.
&term	31 Jul 2005 (sd) : changes to prevent gcc warnings.
&term	03 Oct 2007 (sd) : Cleaned up manual page.
&endterms

&scd_end
------------------------------------------------------------------ */
