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
---------------------------------------------------------------------------
  Mnemonic: 	ng_tumbler.c
  Abstract: 	Routines that assist in the creation of files
		using a two tumbler naming system. This is a generalised
		and narbarlek-ised offshoot of ng_ckpt 
 Date: 		07 Jan 2005
 Author: 	Andrew Hume
---------------------------------------------------------------------------
*/
#include	<string.h>
#include 	<stdlib.h>
#include	<limits.h>
#include 	<sfio.h>
#include	"ningaui.h"
#include	<ng_lib.h>

Tumbler *
ng_tumbler_init(char *nvar)
{
	Tumbler *t;
	char *s, *oq, *q;

	t = ng_malloc(sizeof(*t), "tumbler init");
	t->nvarname = ng_strdup(nvar);
	t->pre = t->suf = 0;
	s = ng_pp_get(t->nvarname, &t->scope);
	if(s == 0 || !*s ){
		sfprintf(sfstderr, "%s: couldn't find pp_get %s\n", argv0, t->nvarname);
		ng_free(t);
		return 0;
	}
	ng_bleat( 2, "ng_tumbler: read tumblerspec(%s) = >%s<\n", t->nvarname, s);

#define	SKIP(eos)	{ for(oq = q; *q; q++) if(*q == ',') break; if((*q == 0) != eos) goto err; *q++ = 0; }
	q = s;
	SKIP(0); t->pre = ng_strdup(oq);
	SKIP(0); t->suf = ng_strdup(oq);
	SKIP(0); t->lo_now = strtol(oq, 0, 10);
	SKIP(0); t->lo_max = strtol(oq, 0, 10);
	SKIP(0); t->hi_now = strtol(oq, 0, 10);
	SKIP(1); t->hi_max = strtol(oq, 0, 10);

	if( t->lo_max <= 0 )			/* prevent core dump if user buggers the pp var */
		t->lo_max = 10;
	if( t->hi_max <= 0 )
		t->hi_max = 10;
	return t;
err:
	sfprintf(sfstderr, "%s: tumbler spec parsing error at '%s'\n", argv0, q);
	if(t->pre)
		ng_free(t->pre);
	if(t->suf)
		ng_free(t->suf);
	ng_free(t->nvarname);
	ng_free(t);
	return 0;
}

char *
ng_tumbler(Tumbler *t, int managed, int peek, int peek_offset)
{
	char *s;
	int slen;
	int hi, lo;

	if( !t )
	{
		ng_bleat( 0, "ng_tumbler: tumbler pointer was null" );
		return NULL;
	}
	
	slen = strlen(t->pre) + strlen(t->suf) + 6 /* .b345. */ + 1 /* null */;
	s = ng_malloc(slen, "tumbler");
	hi = t->hi_now;
	lo = t->lo_now;
	hi += (peek? peek_offset : 1);
	while(hi < 0){
		hi += t->hi_max;
		lo--;
	}
	while(hi >= t->hi_max){
		hi -= t->hi_max;
		lo++;
	}
	while(lo < 0)
		lo += t->lo_max;
	lo = lo % t->lo_max;
	if(hi == 0)
		sfsprintf(s, slen, "%s.a%03d.%s", t->pre, lo, t->suf);
	else
		sfsprintf(s, slen, "%s.b%03d.%s", t->pre, hi, t->suf);
	if(peek)
		return s;
	t->hi_now = hi;
	t->lo_now = lo;
	if(t->nvarname){
		char *v;

		slen += 100;		/* allow for maxes */
		v = ng_malloc(slen, "tumbler,nar");
		sfsprintf(v, slen, "%s,%s,%d,%d,%d,%d", t->pre, t->suf, t->lo_now, t->lo_max, t->hi_now, t->hi_max);
		if(ng_pp_set(t->nvarname, v, t->scope) == 0)
			sfprintf(sfstderr, "%s: error in pp_setting narvar %s(%d)\n", argv0, t->nvarname, t->scope);
		ng_free(v);
	}
	if(managed){
		char *x;

		x = ng_spaceman_nm(s);
		if(x == 0)
			sfprintf(sfstderr, "%s: spaceman(%s) failed\n", s);
		ng_free(s);
		s = x;
	}
	return s;
}


#ifdef SELF_TEST
main( int argc, char **argv )
{
	int i;
	char *s1, *s2;
	Tumbler *t;
	int	peek = 1;

	argv0 = "ng_tumbler_test";
	verbose = 1;

	if(argc < 2){
		ng_bleat( 0, "Usage: %s nvarname [0]", argv0);
		ng_bleat( 0, "if parm 2 is 0, then peek mode is off");
		ng_bleat( 0, "\tpeek==0 to acutally change nvarname in pinkpages" );
		exit(1);
	}

	t = ng_tumbler_init(argv[1]);
	if(t == 0){
		ng_bleat( 0, "tumbler_init failed");
		exit(1);
	}

	if( argc > 2 )
		peek = atoi( argv[2] );

	ng_bleat( 0, "testing tumbler for %s (peek mode =%d)", argv[1], peek );
	for( i = 0; i < 20; i++ )
		ng_bleat( 0, "%s", ng_tumbler(t, 0, peek, 1));
	ng_bleat( 0, "testing tumbler for %s (managed)", argv[1] );
	for( i = 0; i < 20; i++ )
		ng_bleat( 0, "%s", ng_tumbler(t, 1, peek, 1));

	ng_bleat( 0, "testing tumbler for %s (peek mode offset=3)", argv[1] );
	ng_bleat( 0, "%s", ng_tumbler(t, 0, 1, 3));
}
#endif



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_tumbler:Tumbler File Name Support)

&space
&synop	
	#include <ningaui.h>
&break	#include <ng_lib.h>
&break	Tumbler *ng_tumbler_init(char *nvar)
&break	char *ng_tumbler(Tumbler *t, int managed, int peek, int peek-offset )


&space
&desc
	These routines provide a consistent method of creating so-called
	"tumbler" file names, such as commonly used for checkpoint files.
	With these routines, all the tumbler information, including the current
	state, is stored within a pink pages variable. See &ital(ng_tumbler)
	(tool) for the details on setting the associated pinkpages variable.
&space
&subcat	ng_tumbler()
	This function returns a character string containing the filename of 
	the next tumbler file.  The pinkpages variable provided on the 
	ng_tumbler_init() call is updated (unless the peek option is set).
	The following parameters are passed to the function:

&space
&begterms
&term	t : The pointer to the tumbler structure returned by the ng_tumbler_init()
	call. 
&space
&term	managed : If non-zero, the filename generated will be in replication manager 
	file space.  If this value is 0, then only a basename is returned. When the 
	peek option is set, this option is ignored. 
&space
&term	peek : Wen set to nonzero, the next filename is returned, however the 
	pinkpages variable is not updated. When the peek option is supplied, the 
	returned string is always a basename; regardless of the managed option value.
&space
&term	peek-offset : Used if the peek option is set.  This causes the nth 
	checkpoint name from the current setting to be generated rather than the 
	next.  To peek at the current setting, 0 should be used.  Negative 
	offsets are allowed. 
&endterms
	

&space
	These routines replace the old &lit(ng_ckpt*) routines.

&space
&see	&ital(ng_tumbler)(tool)

&space
&mods
&owner(Andrew Hume)
&lgterms
&term 	10 Jan 2004 (agh) : New code, based on ng_ckpt.
&term	31 Jul 2005 (sd) : changes to eliminate gcc warnings.
&term	12 Jun 2006 (sd) : Added doc. Corrected self test call to ng_tumbler() (still 
	old ng_ckpt() format).
&term	15 Jun 2006 (sd) : Added check to see if *t is null to prevent core dump.
&term	27 Oct 2006 (sd) : Added check of return value from pp_get() to see if the string is empty.
&endterms

&scd_end
------------------------------------------------------------------ */
