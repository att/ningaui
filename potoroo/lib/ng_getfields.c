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
	mod:	31 July 2005 (sd) : changes to fix return types (char ** defaulting to int bad on 64 bit nodes)
*/
#include	<ctype.h>
#include	<sfio.h>
#include	<string.h>
#include	"ningaui.h"

static char is_sep[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
static char is_field[256] = {
	0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};
static char last_sep[256];

char *
ng_setfields(char *arg)
{
	unsigned char *s;
	int i;

	for(i = 1, s = (unsigned char *)last_sep; i < 256; i++)
		if(is_sep[i])
			*s++ = i;
	*s = 0;
	memset(is_sep, 0, sizeof is_sep);
	memset(is_field, 1, sizeof is_field);
	for(s = (unsigned char *)arg; *s;){
		is_sep[*s] = 1;
		is_field[*s++] = 0;
	}
	is_field[0] = 0;
	return(last_sep);
}

int
ng_getfields(char *ss, char **sp, int nptrs)
{
	unsigned char *s = (unsigned char *)ss;
	unsigned char **p = (unsigned char **)sp;
	unsigned char c;

	for(;;){
		if(--nptrs < 0) break;
		*p++ = s;
		while(is_field[c = *s++])
			;
		if(c == 0) break;
		s[-1] = 0;
	}
	if(nptrs > 0)
		*p = 0;
	else if(--s >= (unsigned char *)ss)
		*s = c;
	return(p - (unsigned char **)sp);
}

int
ng_getmfields(char *ss, char **sp, int nptrs)
{
	register unsigned char *s = (unsigned char *)ss;
	register unsigned char **p = (unsigned char **)sp;
	register unsigned c;

	if(nptrs <= 0)
		return(0);
	goto flushdelim;
	for(;;){
		*p++ = s;
		if(--nptrs == 0) break;
		while(is_field[c = *s++])
			;
		/*
		 *  s is now pointing 1 past the delimiter of the last field
		 *  c is the delimiter
		 */
		if(c == 0) break;
		s[-1] = 0;
	flushdelim:
		while(is_sep[c = *s++])
			;
		/*
		 *  s is now pointing 1 past the beginning of the next field
		 *  c is the first letter of the field
		 */
		if(c == 0) break;
		s--;
		/*
		 *  s is now pointing to the beginning of the next field
		 *  c is the first letter of the field
		 */
	}
	if(nptrs > 0)
		*p = 0;
	return(p - (unsigned char **)sp);
}

#ifdef	MAIN
#include	<fio.h>

main()
{
	char *fields[256];
	char *s;
	int n, i;
	char buf[1024];

	print("go:\n");
	while(s = Frdline(0)){
		strcpy(buf, s);
		Fprint(1, "getf:");
		n = getfields(s, fields, 4);
		for(i = 0; i < n; i++)
			Fprint(1, " >%s<", fields[i]);
		Fputc(1, '\n');
		Fprint(1, "getmf:");
		n = getmfields(buf, fields, 4);
		for(i = 0; i < n; i++)
			Fprint(1, " >%s<", fields[i]);
		Fputc(1, '\n');
		Fflush(1);
	}
	exit(0);
}
#endif


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(get_fields:Buffer Field Parsing)

&space
&synop	
	#include <ningaui.h>
&break	#include <ng_lib.h>
&space
&break	char * ng_setfields(char *arg)
&break	int ng_getfields(char *ss, char **sp, int nptrs)
&break	int ng_getmfields(char *ss, char **sp, int nptrs)

&space
&desc
	The &lit(ng_setfields) function accepts a NULL terminated string of characters that are 
	considered field separators. If this function is not invoked, only the tab ('\t')
	and space characters are considered to be separators. 
&space
	&lit(Ng_getfields) divides the user string (ss) into tokens defined by the separator(s) 
	that have been defined using &lit(ng_setfields.) The address of the start of each token is 
	pointed to using the supplied array of character pointers (sp); &lit(nptrs) is the number of 
	pointers in the array.  The return value is the number of array pointers placed into 
	&lit(sp.)  If &lit(nptrs) is larger than the number of pointers generated, a NULL pointer 
	will terminate the list. 

&space
	&lit(Ng_getmfields) divides the user string (ss) into tokens, however multiple consecutive 
	field separator tokens to not generate null tokens.  The return value is the number of 
	pointers placed into the array &lit(sp.)

&space
	WARNING:
&break	Both the &lit(ng_getfields) and &lit(ng_getmfields) functions alter the user supplied 
	string &lit(ss.)



&space
&see	ng_tokenise(lib)

&space
&mods
&owner(Andrew Hume)
&lgterms
&term	01 Jan 2006 (ah) : Its beginning of time. 
&term	03 Oct 2007 (sd) : Added manual page.
&endterms


&scd_end
------------------------------------------------------------------ */

