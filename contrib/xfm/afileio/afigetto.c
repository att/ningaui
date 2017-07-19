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
/*
***************************************************************************
*
*  Mnemonic: AFIgettoken
*  Abstract: This routine returns the next token from the input buffer
*            of a file that has turn on tokenizing input. (AFIsettoken)
*            If the varsym character was supplied on the settoken call
*            then this routine will expand variables encountered in the 
*            input stream.
*  Parms:    file  - AFI handle of the open file
*            buf   - Pointer to the buffer where token should be placed.
*  Returns:  Length of the token or AFI_ERROR if end of the file was reached. 
*  Date:     28 March 1997
*  Author:   E. Scott Daniels
*
*  Modified:  	02 Apr 1997 - To support token seperator list
*            	27 Aug 2000 - To concat punct (nexttok) butted agaings &var()
*		11 Jan 2002 - To add symbol table and new variable expander
*		08 Nov 2005 - Fixed nested ) problem in &name() expansion
*		09 Apr 2007 - Fixed memory leak (erk).
*		22 Oct 2007 - Fixed bug that truncated first character of leading
*			whitespace when F_WHITE flag is set.
***************************************************************************
*/

#include "afisetup.h"
#include "../lib/parse.h"

int AFIgettoken( AFIHANDLE file, char *buf )
{
	static depth = 0;
	int len = AFI_ERROR;           /* length of the token we are passing back */
	int i;                         /* loop index */
	int j;
	int hit_vsym = 0;
	int hit_paren = 0;
	char *tok;                     /* start of token pointer */
	char *ebuf;			/* pointer to buffer with expanded tokens */
	struct file_block *fptr;       /* pointer at file management block */
	struct inputmgt_blk *imb;      /* pointer directly at imb */
	char tbuf[MAX_TBUF];           /* temp buffer for read */
	int	wsc;			/* holder of whitespace character that caused token break */

	if( depth++ > 25 )
	{
		fprintf( stderr, "afigettoken: something smells; too deep. \n" );
		*buf = EOS;
		if( depth > 25 )
		exit( 1 );
	}
	

	*buf = EOS;                             /* ensure user buffer is zapped */

	if( (fptr = afi_files[file]) != NULL && !(fptr->flags & F_EOF) )            /* good handle and not at end */
	{
		if( fptr->tmb == NULL )
		{
			fprintf( stderr, "afigettoken: input file (%s) not set for tokenising" );
			exit( 1 );
		}

		while( (!(fptr->flags & F_EOF)) && fptr->tmb->ilist->idx >= fptr->tmb->ilist->end ) 
		{
			if( fptr->tmb->ilist->flags & IF_EOBSIG )   		/* end of buffer signal? */
			{
				fptr->tmb->ilist->flags &= ~IF_EOBSIG;    	/* clear so it happens once */
				depth--;
				return( 0 );                              	/* return 0 length token */
			}
			else
			{	
				if( fptr->tmb->ilist->next == NULL )            /* last input buffer */
				{
					while( (len = AFIread( file, tbuf )) == 0 );
					fptr = afi_files[file];         /* in case chain closed and new ptr */
					if( len > 0 )
					{                                    /* non blank record found */
						if( fptr->tmb->ilist->buf )
							free( fptr->tmb->ilist->buf );
						fptr->tmb->ilist->buf = strdup( tbuf );
						fptr->tmb->ilist->idx = fptr->tmb->ilist->buf;    /* reset index */
						fptr->tmb->ilist->end = fptr->tmb->ilist->buf + strlen( fptr->tmb->ilist->buf );
			
						if( fptr->flags & F_EOBSIG)         /* need to mark real buffer? */
							fptr->tmb->ilist->flags |= IF_EOBSIG;
					}
					else
					{
						if( len = -1 && ! (fptr->flags & F_EOF)  && fptr->flags & AFI_F_EOFSIG )
						{
							depth--;
							return -1;		/* close notification */
						}
					}
				}                                     /* not last input buf on stack */
				else             
				{
					imb = fptr->tmb->ilist;
					fptr->tmb->ilist = fptr->tmb->ilist->next;  /* at next on stack */
					if( imb )
					{
						free( imb->buf );
						free( imb );
					}
					imb = NULL;
				}
			}
		}  

		if( fptr &&  ! (fptr->flags & F_EOF) )       /* if not at eof */
		{
			imb = fptr->tmb->ilist;          
			if( !(fptr->flags & AFI_F_WHITE) )				 /* skip lead white if not keeping it */
			{
				for( ; imb->idx < imb->end && isspace( *imb->idx ); imb->idx++ );
				tok = imb->idx;						/* token starts beyond whitespace */
			}
			else
			{
				tok = imb->idx;						/* start of token includes whitespace */
				for( ; imb->idx < imb->end && isspace( *imb->idx ); imb->idx++ );  /* skip to first non-white space */
			}


			while( imb->idx < imb->end && !strchr( fptr->tmb->tsep, *imb->idx ) )  /* find end of the token */
			{
				if( *imb->idx == '^' )		/* does not count ^& or ^( or ^) */
					imb->idx++;
				else
				if( *imb->idx == '&' )
					hit_vsym++;
				else
				if( *imb->idx == '(' )
					hit_paren++;
				else
				if( *imb->idx == ')' )	/* if stuff in &name(stuff) has no seperators, we dont search for ) later */
					hit_paren--;

				imb->idx++;
			}
	
			if( hit_vsym  ) 
			{
				while( *imb->idx && hit_paren >0 )		/* find matching ) if &vname( found */
				{
					if( *imb->idx == '^' )
						imb->idx++;			/* skip next one */
					
					if( *imb->idx == '(' )		/* allow nested parens inside of &name(...) */
						hit_paren++;

					if( *imb->idx == ')' )
						hit_paren--;

					imb->idx++;
				}
			}

			wsc = *imb->idx;		/* save the whitespace char to replace later */
			*imb->idx = 0;			/* mark end of token which will include (stuff stuff) if &vname(stuff stuff) */
  			ebuf=vreplace(fptr->symtab, 0, tok, fptr->tmb->varsym, fptr->tmb->escsym, *fptr->tmb->psep, VRF_CONSTBUF );

			if( strcmp( ebuf, tok ) != 0 )	/* var expanded to something, push it on the stack and recurse to return 1st tok */
			{
				AFIpushtoken( file, ebuf );		
				len = AFIgettoken( file, buf );
			}
			else
			{
				/*strcpy( buf, ebuf );*/
				for( i = 0, j = 0; ebuf[i]; i++ )
				{
					if( ebuf[i] == fptr->tmb->escsym && ebuf[i+1] == fptr->tmb->varsym )
						i++;		/* strip ^& to & */
					buf[j++] = ebuf[i];	
				}
				buf[j] = 0;

				len = strlen( buf );
			}
			
			if( wsc == ')' )			/* we must skip if it was this */
				imb->idx++;		
			else
				*imb->idx = wsc;		/* replace whitespace; need this in case they stop skipping whitespace */
		}

	}

	/*fprintf( stderr, " returning depth=%d len=%d buf=(%s) \n", depth, len, buf  );   */
	depth--;
	return len;
}                      /* AFIgettoken */

