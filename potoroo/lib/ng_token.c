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
* Mnemonic:	ng_tokeniser and ng_readtokens
* Abstract:	Constructs an array of pointers to the tokens in a buffer.
*		The array returned is created as the lead part of a block
*		of memory that contains both the pointer array and token 
*		strings.  Thus a single free on the array will release all
*		of the memory that was allocated for this.
*		Note: We dont use strtok cuz it skips empty tokens like #2
*		in: fred::wilma:betty:barny
* Returns: 	pointer to array of character pointer
* Date:		27 Aug 2001
* Author:	E. Scott Daniels
--------------------------------------------------------------------------
*/
#include	<unistd.h>
#include	<stdio.h>
#include	<string.h>
#include	<errno.h>

#include	<sfio.h>
#include	<ningaui.h>
#include	<ng_basic.h>
#include	<ng_lib.h>
#include	<ng_ext.h>

struct tk_blk {
	char	*toks[512];		/* should be plenty */
	char	str[NG_BUFFER];
};

char **ng_tokenise( char *b, char sep, char esc, char **reuse, int *count )
{

	struct	tk_blk *tkb;
	int	i = 0;
	char	*src;
	char	*dest;

	if( reuse )
		tkb = (struct tk_blk *) reuse;
	else
		tkb = ng_malloc( sizeof( struct tk_blk), "token block" );

	memset( tkb, 0 , sizeof( struct tk_blk ) );

	dest = tkb->str;
	src = b;

	tkb->toks[i++] = dest;
	*dest = 0;
	while( i < 512 && *src && src - b < NG_BUFFER )
	{
		if( *src == sep )
		{
			*dest++ = 0;		/* end the current token */
			tkb->toks[i++] = dest;
			src++;
		}
		else
		{
			if( *src == esc )
				src++;

			*dest++ = *src++;
		}
	}

	if( i >= 512 )			/* error if we stopped for either of these reasons */
	{
		errno = E2BIG;
		ng_free( tkb );
		return NULL;
	}

	if( src - b > NG_BUFFER )
	{
		errno = ENOMEM;
		ng_free( tkb );
		return NULL;
	}

	*dest = 0;			/* ensure last token is terminated */
	if( count )
		*count = i;		/* number of tokens */

	return (char **) tkb;
}

/* read and tokenise the buffer. Returns null if at end of file, and frees the reuse
   buffer if one was supplied 
*/
char **ng_readtokens( Sfio_t *f, char sep, char esc, char **reuse, int *count )
{
	char	*buf;

	if( (buf = sfgetr( f, '\n', SF_STRING )) != NULL )
		return ng_tokenise( buf, sep, esc, reuse, count );
	else
	{
		if( reuse )
			ng_free( reuse );

		if( count )
			*count = 0;

		return NULL;
	}
}




#ifdef SELFTEST
int main( )
{
	char	*buf;
	char	**toks = NULL;
	int	count;

	/* read input and echo tokens out reveresed */
	sfprintf( sfstderr, "enter a string with tokens separated by colons; tokens should be echoed out in REVERSE order\n" );
	while( (toks = ng_readtokens( sfstdin, ':', '\\', toks, &count )) != NULL )
	{
		sfprintf( sfstdout, "got %d tokens: ", count );
		while( --count >= 0 )
			sfprintf( sfstdout, "(%s) ", toks[count] );
		sfprintf( sfstdout, "\n" );
	}
}
#endif


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_tonekise:String Tokenising Routines)

&space
&synop	char **ng_tokenise( char *b, char sep, char esc, char **reuse, int *count )
&break	char **ng_readtokens( Sfio_t *f, char sep, char esc, char **reuse, int *count )

&space
&desc	&ital(Ng_tokenise) parses the buffer pointed to by &ital(b) and 
	creates a list of the tokens contained within the buffer. Tokens 
	are considered to be all characters between two seperator characters
	(&ital(sep)) or between the beginning of the buffer and the next 
	seperator character, or the last seperator character and the end of 
	the buffer. Each of these routines returns a pointer to an array of
	character pointers, each character pointer pointing to a single token,
	with each token being a NULL terminated string.
	The source buffer (passed to &lit(ng_tokenise)) is left unchanged,
	and can be modified by the caller without affecting the tokens.
&space
	The memory allocated by these routines is done such that the user 	
	need free &stress(only) the pointer returned rather than each of 
	the pointers in the token array.  If the user is making multiple calls
	to these routines, and does not need to have multiple concurrent sets 
	of tokens, the token list pointer can be passed into these routines
	(&ital(reuse)) and the storage will be reused.  
&space
	If the escape character is not zero (0), then when encountered 
	in the input buffer it is skipped and the next character is placed 
	into the token. Thus if the seperator is defined as the colon character, 
	and the backslash as the escape character, the buffer 
	&lit(abc:de\:fg) would be parsed into two tokens: &lit(abc) and 
	&lit(de:fg). 

&space
	The only difference between &lit(ng_tokenise) and &lit(ng_readtokens) 
	is that &lit(ng_readtokens) expects to read a newline terminated buffer
	from the file &ital(f) rather than receiving a pointer to the buffer. 


&space
&parms	The following parameters are accepted by one, or both, of these 
	routines:
&begterms
.di	b : Pointer to the buffer to parse. The buffer is assumed to 
	be a NULL terminated string not exceeding &lit(NG_BUFFER) characters.
&space
.di 	f : Pointer to an &lit(Sfio_t) structure that represents an open 
	file, positioned for the next read. Records will be read from 
	the current position until the end of file.
&space
.di 	sep : The character that is used to seperate tokens in the source.
&space
.di 	esc : The character that is used to escape the seperator character 
	if necessary. Can be 0 if no escape character is to be supported.
&space
.di 	reuse : Pointer returned by a previous call to either &lit(ng_tokenise)
	or &lit(ng_readtokens). The memory allocated by the previous call
	will be reused. If NULL, a new allocation will be created.
&space
.di 	count : Pointer to an integer in which either routine can 
	return the number of tokens that are contained in the list. If 
	no count is desired, then the caller may set this to NULL. 
&endterms

&space
&return	These routines return a pointer that the user can treat as a pointer
	to an array of character pointers (char **), or NULL if there was 
	an error. If the return value is NULL, &lit(errno) is set to 
	reflect the problem encountered.
&space
&begterms
.di	E2BIG : More than 512 tokens were contained in the input buffer.
&space
.di	ENOMEM : The input buffer was larger than &lit(NG_BUFFER). 
&endterms

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	27 Aug 2001 (sd) : Initial routines for use with stream depot.
&term	19 Oct 2001 (sd) : Converted for library.
&term	31 Jul 2005 (sd) : changes to prevent gcc warnings.
&endterms

&scd_end
------------------------------------------------------------------ */
