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
*  Mnemoic:	ng_pp_expand
*  Abstract:	Replace variables (e.g. $name) in a buffer with the value that the name
*		maps to in the pinkpages. 
*		These are legal expansions:
*			$scooter
*			${scooter}junk
*			junk$scooter
*
*  Parms:	
*		sbuf - source buffer 
*		vsym - variable indicator % $ etc
*		esym - escape symbol so that \% will cause a % to go into the expanded buf
*			
*		flags:	0x01 - create a new buffer for expansion; user must free
*			0x02 - Do NOT do recursive expansion on variables
*			0x03 - cache pinkpages variables (faster)
*			(we use VEX_ flag constants -- shared with ng_var_exp() )
*  Returns:	Pointer to the expanded buffer
*  Date:	12 Sep 2007
*  Author: 	E. Scott Daniels
* ---------------------------------------------------------------------------------------
*/

#include	<unistd.h>
#include	<stdio.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<ctype.h>

#include	<sfio.h>

#include	<ningaui.h>
#include	<ng_lib.h>
#include	<ng_ext.h>		/* VEX_ constants live here */

#define F_BRACKETED	0x01		/* name was encased in {}. we must replace them if name not in tab */

#define MAX_NAME	1024		/* max size of a name */
#define MAX_BUF		4096		/* max buffer size */

static	char vreplace_buffer[MAX_BUF];	/* static buffer whose address is returned if user asks for a const buf */


/* look up a variable either in pinkpages, or the cache */
static char *lookup( char *vname, int use_cache )	
{
	static void 	*ncache = NULL;
	static char	me[1024];
	static	char	*cluster = NULL;

	int 	i;
	char	*p;
	char	*scope;
	char	*value;

	if( use_cache )
	{
		char ppvname[1024];

		if( ! ncache )
		{
			ncache = ng_nar_cache_new( "pinkpages", 0 );		/* open cache and refresh at default rate */
			ng_sysname( me, sizeof( me ) );
			if( (p = strchr( me, '.' )) )
				*p = 0;
			cluster = ng_env( "NG_CLUSTER" );
		}

		if( ncache && cluster )
		{
			for( i = 0; i < 4; i++ )
			{	
				switch( i )
				{
					case 0: scope = me; break;			/* local look up first */
					case 1: scope = cluster; break;			/* try cluster var next */
					case 2: scope = "flock"; break;			/* one last go for flock */
					default: 	return NULL;			/* nada - get out now */
				}

				sfsprintf( ppvname, sizeof( ppvname ), "pinkpages/%s:%s", scope, vname );
				if( (value = ng_nar_cache_get( ncache, ppvname )) )
					return value;
			}
		}
	}

	return ng_pp_get( vname, NULL );
}


/* most will use pp_expand, but this interface offers complete control
   similar to the var_exp() function.
*/
char *ng_pp_expand_x( char *sbuf, char vsym, char esym, int flags )
{
	static int depth = 0;		/* prevent a never ending recursion by aborting if we go too deep */

	char	name[MAX_NAME+1];	/* spot to save a var name for look up */
	char	pname[MAX_NAME+1];	/* parameter name buffer */
	char	*obuf = NULL;		/* pointer to buf to return */
	char	*optr = NULL;		/* pointer at next insert point in output */
	char	*vbuf;			/* pointer to variable value string */
	char 	*oend;			/* prevent buffer overruns */
	char	*ebuf;			/* full expansion of variable - ready for cp to obuf */
	char	*eptr;			/* pointer into ebuf */
	int 	iflags = 0;		/* internal flags, not to be confused with VEX flags passed in as flags  */
	int	nidx;			/* index into name buffer */
	int	pidx;			/* parameter list index */
	int	p;			/* parameter number */


	if( ++depth > 25 )
	{
		ng_bleat( 0, "pp_exp: panic: too deep" );
		ng_bleat( 0, "pp_exp: sbuf=(%s)", sbuf );
		ng_bleat( 0, "pp_exp: flags=0x02x", flags );
		ng_bleat( 0, "pp_exp: vsym=%c", vsym );
		abort( );
	}

	if( flags & VEX_NEWBUF )
		obuf = (char *) ng_malloc( MAX_BUF-1 * sizeof( char ), "pp_exp, buffer" );
	else
		obuf = vreplace_buffer;

	optr = obuf;
	oend = obuf + (MAX_BUF-1);

	while( sbuf && *sbuf && (optr < oend) )	
	{
		if( *sbuf == vsym )
		{
			nidx = 0;
			sbuf++;
			if( *sbuf == '{' )
			{
				iflags |= F_BRACKETED;
				sbuf++;
				while( nidx < MAX_NAME && *sbuf && *sbuf != '}' )
					name[nidx++] = *sbuf++;
				sbuf++;					/* step over end } */
				name[nidx] = 0;
			}
			else
			{
				while( nidx < MAX_NAME && *sbuf && (isalnum( *sbuf ) || *sbuf == '_' ) )
					name[nidx++] = *sbuf++;
				name[nidx++] = 0;
			}

			if( (vbuf = lookup( name, flags & VEX_CACHE )) )
			{
				if( flags & VEX_NORECURSE )		/* recursion is not allowed */
				{				
					eptr = vbuf;
					ebuf = NULL;		/* free will not barf */
				}
				else				/* recurse to expand anything thats left */
					eptr = ebuf = ng_pp_expand_x( vbuf, vsym, esym, VEX_NEWBUF );

				while( *eptr && (optr < oend) )		/* add expanded stuff to output */
					*optr++ = *eptr++;	

				ng_free( vbuf );
				ng_free( ebuf );
			}
			else					/* name not in symtab */
			{
				*optr++ = vsym;
				if( iflags & F_BRACKETED )
					*optr++ = '{';			/* must replace these too */
				eptr = name;				/* just copy the name in  with lead var sym */
				while( *eptr && (optr < oend) )		/* add expanded stuff to output */
					*optr++ = *eptr++;	
				if( iflags & F_BRACKETED )
					*optr++ = '}';			/* must replace these too */
			}
		}
		else
		if( *sbuf == esym )
		{
			sbuf++;			/* skip it and add the next character as is */
			*optr++ = *sbuf++;
		}
		else
			*optr++ = *sbuf++;	/* nothing special, just add the character */
	}

	*optr = 0;
	depth--;
	return obuf;
}

/* ease of use withtout the control */
char *ng_pp_expand( char *sbuf )
{
	return ng_pp_expand_x( sbuf, '$', '\\', VEX_CACHE|VEX_NEWBUF );
}

#ifdef SELF_TEST

int main( int argc, char **argv )
{
	char	*val;
	char	*eval;
	int	i;

	for( i = 0; i < argc; i++ )
	{
		if( argv[i+1] == NULL )
			val = ng_strdup( "$NG_ROOT" );			/* this should work! */
		else
			val = ng_pp_get( argv[i+1], NULL );

		if( val ) 					/* (val = ng_pp_get( argv[i+1], NULL )) )*/
		{
			/*eval = ng_pp_expand_x( val, '$', '\\', VEX_CACHE | VEX_NEWBUF );*/
			eval = ng_pp_expand( val );
			printf( "%s = (%s) ==> (%s)\n", argv[i+1], val, eval );
			ng_free( eval );
			ng_free( val );
		}
		else
			printf( "%s = (%s)\n", argv[i+1], "undefined in pinkpages" );
	}
}
#endif



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_pp_expand:Pinkpages Variable Expander)

&space
&synop	
	#include <ningaui.h>
&break	#include <ng_ext.h>
&break
&break	char *ng_pp_expand( char *sbuf )	
&break	char *ng_pp_expand_x( char *sbuf, char vsym, char esym, int flags )

&space
&desc	&This parses the &ital(sbuf) source buffer and expands any variables using the 
	value that the variable name maps to within the &ital(pinkpages) environment. 
	The a pointer to the resulting expanded string is returned.
	A variable name is recognised in the source buffer as any string that begins with  a dollar sign ($) character and 
	continuing until a non-alphanumeric character is encountered. (An underbar character is considered to be alpha-numeric.) 
	Using the curly brace characters ({ and }), a variable name may be concatinated to text in the same 
	mannar as is accomplished in the Korn shell (e.g. &lit(${foo}bar) would expand the variable foo and concatinate the 
	characters &lit(bar) to the end).

&space
	The &lit(ng_pp_expand_x) function offers the calling programme more control in terms of defining the variable ID character
	(vsym), the escape character (esym), and allowing any of the &lit(VEX_) flag constants (see ng_ext.h) to be supplied. 
	The variable symbol (&ital(vsym)) is ignored if it is immediately preceded with the escape character (&ital(esym)).
	If a variable name is not defined in the symbol table, then the unexpanded name (with the variable symbol) is 
	placed into the expanded buffer.
	&space
	This invokes &lit(ng_pp_expand_x) with a variable symbol of '$' and an escape symbol of '\'.  The VEX constants used by 
	&this are VEX_CACHE and VEX_NEWBUF. 

&space
	The expansion process is recursive unless the &lit(VEX_NORECURSE) flag is used. The funciton will cause a process abort
	if the expansion decends more than 25 recursions. 

&space
&subcat Caching
	The &lit(VEX_CACHE) flag causes the pinkpages expansion routines to create a cache of pinkpages data
	that is used to do the expansions.  This can significantly reduce the number overhead in that the interaction 
	with narbalek is kept to a minimum.  When using the cache feature, the cache is refreshed if it is more than 
	two minutes old.  This allows long running programmes to use the caching feature without running into issues
	with stale variables.  Programmes can choose to disable caching completely by using the &lit(_x) form of the 
	function and will always get the latest pinkpages variables from narbalek.

&space
&parms	The following parameters are expected to be passed into &this:
&begterms
&term	sbuf : A pointer to a null terminated buffer containing the character string to parse.
&space
&term	vsym : The character that is to be recognised as the variable reference symbol.
&space
&term	esym : The character that is to be recognised as the escape symbol.
&space
&term	flags : Any of the VEX flag constants OR'd together:
&indent
	&beglist
	&term	VEX_CONSTBUF	: Causes &this to expand the string into a static buffer. This is the default if no 
		flags are supplied.
	&space
	&term	VEX_NEWBUF 	: &This returns the expanded string in a dynamic buffer. The user must free the buffer
		to prevent memory leaks.
	&space	
	&term 	VEX_NORECURSE :	If this is set, &this will &stress(not) recurse to expand variable references that 
		are contained in the initially expanded variable value. 
	&space
	&term	VEX_CACHE : Allow caching of pinkpages data for faster lookups. 
	&endlist
&uindent
&endterms

&space
&exit	&This always returns a pointer to a buffer. There may be variable references in the buffer that were 
	not expanded. Thes are left in their variable name form (rather than inserting null space). 

&see
	ng_var_exp, ng_pp_get, ng_pp_set, ng_nar_get, ng_nar_set, ng_nar_cache_new, ng_nar_cache_get
&space
&mods
&owner(Scott Daniels)
&lgterms
&term	12 Sep 2007 (sd) : Something new.
&endterms

&scd_end
------------------------------------------------------------------ */
