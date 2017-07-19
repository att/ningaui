/*
* ---------------------------------------------------------------------------
* This source code is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* If this code is modified and/or redistributed, please retain this
* header, as well as any other 'flower-box' headers, that are
* contained in the code in order to give credit where credit is due.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* Please use this URL to review the GNU General Public License:
* http://www.gnu.org/licenses/gpl.txt
* ---------------------------------------------------------------------------
*/

#include <stdio.h>     
#include <stdlib.h>
#include <fcntl.h>    
#include <ctype.h>   
#include <string.h> 
#include <memory.h>
#include <time.h>

#include "../lib/symtab.h"		/* our utilities/tools */
#include "../afileio/afidefs.h"   


#include "fmconst.h"               /* constant definitons */
#include "rfm_const.h"


#include "fmcmds.h"
#include "fmstruct.h"              /* structure definitions */
#include "fmproto.h"

/*
*************************************************************************
*
*   Mnemonic: FMaddtok
*   Abstract: This routine adds a token to the output buffer. If necessary
*             the buffer is flushed before the token is added.
*   Params:   buf - pointer to the token to add
*             len - length of the token
*   Returns:  Nothing.
*   Date:     15 November 1988
*   Author:   E. Scott Daniels
*
*   Modified:  3 May 1992 - To support postscript output
*             11 Nov 1992 - To escape ( and ) in tokens
*             12 Nov 1992 - To call justify inplace of flush if flag is set
*              9 Mar 1993 - To  look for a ^ which is used to escape the
*                            next character.
*              6 Apr 1994 - To use point based line length
*	       9 Feb 2002 - Added ability to chop words with a hiphenator
*	      26 May 2002 - To use the hyphenation library function to see if
*			the word was in the hyph dictionary.
*************************************************************************
*/
static char *hyph_get( char *buf )		/* routine is lost, for now we always return null */
{
	return NULL;
}

void FMchop( char *buf, int len, int remain )
{
	char	t1[1024];
	char	t2[1024];
	char 	*word;
	int	i;
	int	toksize;
	char 	*h;			/* already a hyphd word */

	flags3 &= ~ F3_HYPHEN;			/* turn off while inside here */

	if( (word = hyph_get( buf )) != NULL )
	{
		TRACE(1) "chop: hyph_get returned (%s)\n", word );
		if( h = strchr( word, '-' ) )
		{
			i = (h - word)+1;
			strncpy( t1, word, i );
			t1[i] = 0;
			strcpy( t2, h+1 );
		}
		else
		{
			strcpy( t1, word );
			t2[0] = 0;
			i = strlen( t1 );
		}
	}
	else
	{
		if( (h = strchr( buf, '-' )) )
		{
			i = (h - buf)+1;
			strncpy( t1, buf, i );
			t1[i] = 0;
			strcpy( t2, h+1 );
		}
		else
		{
			for( i = 0; i < len-1 && buf[i] != buf[i+1]; i++ );	/* split at double character */
			i++;
			if( i >= len )		/* no double charcter - find first vowel and split after that */
			{
				fprintf( stderr, "guessing at hyphen: %s\n", buf );
				for( i = 1; i < len-1 && ! strchr( "aeiou", *(buf+i) ); i++ );
				i++;
				}
		
			strncpy( t1, buf, i );
			t1[i] = '-';
			t1[i+1] = 0;
			strcpy( t2, buf+i );
		}
	}

	toksize = FMtoksize( t1, i+1 );
	if( toksize < remain )
	{
		FMaddtok( t1, i + 1 );
		FMaddtok( t2, strlen( t2 ) );
	}
	else
		FMaddtok( buf, len );

		flags3 |= F3_HYPHEN;
}

void FMaddtok( char *buf, int len )
{
	static ok1 = 1; 	/* ok to add a single char at end w/o remain check (for font change .) */
	int remain;          /* calculated space remaining before the right margin */
	int i;               /* loop index */
	int toksize;         /* size of token in points */
	char wbuf[2048];

	len = strlen( buf );
 	words++;             /* increase the number of words in the document */

 	if( buf[0] == '^' )   /* escaping character? */
	{
		buf++;              /* skip over it and decrease the size */
		len--;
	}

	if( flags3 & F3_IDX_SNARF )		/* add to index if snarfing */
		di_add( buf, page + 1 );

	if( optr == 0 )   		    /* if starting at beginning */
		osize = 0;			/* reset current line size */
	else
 		if( flags2 & F2_SMASH ) 
		{
			while( optr && obuf[optr-1] == ' ' )
			{
				optr--;
				osize--;
			}
			obuf[optr] = 0;
		}

	flags2 &= ~F2_SMASH;

	remain = linelen - osize;    /* get space remaining for insertion */

	if( (toksize = FMtoksize( buf, len )) > remain+4 )   /* room for it? */
 	{                                    /* no - put out current buffer */
		TRACE( 1 ) "addtok: len=%d ok1=%d (%s) (%s)\n", len, ok1, buf, obuf );
		if( len == 1 && ok1 )
		{
			ok1 = 0;
		}
		else
		{
			ok1 = 1;
			if( (flags & JUSTIFY ) && (flags3 & F3_HYPHEN) && len > 5  )	/* might try hyphen */
			{
				words -= 2;		/* we already counted; prevent counting the two split tokens */
				FMchop( buf, len, remain );
				return; 
			}

			TRACE(2) "addtok: %s: (%s)\n", flags & JUSTIFY ? "justifying" : "flushing", obuf );
	
			if( flags & JUSTIFY )
				FMjustify( );            /* sendout in justification mode */
			else
			{
				FMflush( );        /* if not justify - send out using flush  */
			}

			osize = 0;          /* new buffer has no size yet */
		}
  	}

 	if( optr == 0 && len == 1 && *buf == ' ' )
		return;

 	if( optr == 0 && *buf == ' ' )			/* squelch lead blanks at the beginning of a buffer */
 	{
		while( *buf++ == ' ' );
		if( *buf == 0 )
			return;
 		toksize = FMtoksize( buf, len );		/* need to recalc */
 	}

 	osize += toksize;         /* add current token to current length */

	for( i = 0; i < len && buf[i]; i++, optr++ )     /* copy token to output buffer */
	{
		if( buf[i] == '(' || buf[i] == ')' || buf[i] == '\\' ) /* need to escape */
		{
			obuf[optr] = '\\';       /* put in the PostScript escape character */
			optr++;                  /* and bump up the pointer */
		}

		obuf[optr] = buf[i];         /* copy the next char to the output buffer */
	}

	obuf[optr]= BLANK;            /* terminate token with a blank */
	optr++;                       /* set for location of next token */

	if( flags2 & F2_PUNSPACE )    /* if need to space after punctuation */
		if( ispunct( obuf[optr-2] ) )  /* and last character is punctuation */
		{
			obuf[optr] = BLANK;           /* add extra space */
			optr++;                       /* and up the pointer */
		}

	obuf[optr] = EOS;             /* terminate buffer incase we flush */
}                 /* FMaddtok */

