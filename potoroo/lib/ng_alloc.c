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

#include	<sfio.h>
#include	<stdlib.h>
#include	<string.h>
#include	"ningaui.h"

static int debugging = -100;
static Sfio_t *fp = 0;

void *
ng_malloc(int n, char *s)
{
	void *x;

	if((debugging < 0) && (++debugging == -1)){
		char *fn;

		if( (fn = ng_env("NG_MALLOC_DEBUG")) ){
			if((fp = ng_sfopen(0, fn, "w")) == NULL){
				sfprintf(sfstderr, "%s: malloc trace file open error: %s (%s)\n", argv0, n, fn, ng_perrorstr());
			}
			sfset( fp, SF_LINE, 1 );		/* flush on each newline */
			debugging = 1;
		} else
			debugging = 0;
	}
	if((x = malloc(n)) == 0){
		sfprintf(sfstderr, "%s: malloc(%d) failed while allocating %s (%s)\n", argv0, n, s, ng_perrorstr());
		abort();
		exit(1);	/* in case the abort returns */
	}
	if(fp)
		sfprintf(fp, "%I*d malloc %d <%s>\n", sizeof(x), x, n, s);
	return x;
}

char *
ng_strdup(char *str)
{
	int n;
	char *x;

	n = strlen(str)+1;
	x = ng_malloc(n, "strdup");
	memcpy(x, str, n);
	if( fp )
		sfprintf(fp, "%I*u strdup <%s>\n", sizeof(x), x, x);			/* write the string to trace file */

	return x;
}

void
ng_free(void *p)
{
	if( !p )
		return;

	free(p);
	if(fp)
		sfprintf(fp, "%I*d free\n", sizeof(p), p);
}

void *
ng_realloc(void *p, int n, char *s)
{
	void *x;

	x = (p == 0)? malloc(n) : realloc(p, n);
	if(x == 0){
		sfprintf(sfstderr, "%s: realloc(%d) failed while reallocating %s (%s)\n", argv0, n, s, ng_perrorstr());
		abort();
		exit(1);	/* in case the abort returns */
	}
	if(fp){
		sfprintf(fp, "realloc %I*d to %d at %I*d <%s>\n", sizeof(p), p, n, sizeof(x), x, s); 
		/*sfprintf(fp, "realloc %I*d to %d at %I*d <%s>\n", sizeof(p), p, n, sizeof(x), x, s);  dont know why there were 2*/
	}
	return x;
}

/*
#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&doc_title(ng_alloc:Memory allocation routines)

&space
&synop
	extern char *argv0
&space
&break	void * ng_malloc(int n, char *s)
&break	void * ng_realloc(void *p, int n, char *s)
&break	char * ng_strdup( char *str )
&break	void ng_free(void *s)
	

&space
&desc	
	These functions provide the Ningaui interface to the corresponding 
	standard C library functions.  In addition to providing the standard allocation
	and reallocation functions, these functions verify that the request was satisified
	and cause the programme to abort when memory cannot be allocated.  This eliminates 
	the need for the calling programme to check the return status and simplifies its 
	code. 
&space
	The use of the string passed to the allocation and
	reallocation funcitons allows these routines to provide a measure of 
	diagnostics information in the event that the request cannot be satisified.
&space
	In addition, when the environment variable &lit(NG_MALLOC_DEBUG) is set to a filename, 
	string duplication, allocation, reallocation and free activity is recorded into the file. 
	This can allow for post execution analysis of memory usage and a quick 
	method for memory leak detection.  

	&ital(Ng_malloc(^)) will allocate &lit(n) bytes using the system &lit(malloc(^)) function
	and will abort the programme if a NULL pointer is returned.  The pointer to the allocated 
	memory is returned. 

&space
	&ital(Ng_realloc(^)) will reallocate the memory pointed to by &lit(p) such that it is at least
	&lit(n) bytes long. Like &lit(ng_malloc,) this function will abort if the request cannot be 
	completed. 

&space
	&ital(Ng_free(^)) should be used to release memory allocated with 
	&ital(ng_malloc(^).) This allows any tracing that is being logged into the file indicated 
	by the environment variable &lit(NG_MALLOC_DEBUG) to be complete. 

&space
	&ital(Ng_strdup) will duplicate the NULL terminated character string passed in.
	

	

&space
&mods
&owner(Andrew Hume)
&lgterms
&term	xx xxx 1997 (ah) : Original code.
&term   1 Oct 1997 (sd) : SCD added.
&term	8 Oct 1997 (ah): support NULL arg in realloc
&term	27 Apr 2001 (sd) : One last conversion tweek in SCD.
&term	23 Jul 2003 (sd) : added ng_strdup here (rather than strdup.c) to give more data 
	when malloc trace is on.
&term	16 Nov 2006 (sd) : Conversion of sfopen() calls to ng_sfopen().
&term	03 Oct 2007 (sd) : Rewrote man page.
&endterms
&scd_end
*/
