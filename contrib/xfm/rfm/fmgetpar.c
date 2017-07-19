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
****************************************************************************
*
*  Mnemonic: FMgetparm
*  Abstract: This routine gets a parameter from the current buffer.
*            If the end of the input buffer is reached before a
*            command parameter is located then a length of 0 is
*            returned. Unlike FMgettok this routine does not continue
*            on to the next input line looking for the parameter.
*            When a single colon (:) is encountered get parm interprets
*            it as the end of the parameter list and returns 0 as if the
*            end of the buffer was reached. This allows commands to be
*            "stacked" on the same line without having to supply all optional
*            parameters.
*            NOTE: This routine is recursive when a variable is encountered
*                  in the parameter list.
*  Parms:    buf - Pointer to a character pointer to return parm pointer in
*  Returns:  Length of the parameter, or 0 if no parameter encountered.
*  Date:     16 November 1988
*  Author:   E. Scott Daniels
*
*  Modified: 7-07-89 - To support variable expansion.
*            9-01-89 - To support user defined variable delimiter.
*            3-29-93 - To support expressions as variables in commands
*            4-26-93 - To allow starting [ to be butted with first token of
*                      the expression.
*            7 Apr 94- To allow : to stop processing.
*            3 Apr 97- To use new AFI tokenizer!
*            8 Apr 97- To support `xxx xxx` tokens in a parm list 
*	    19 Oct 06- Addded parm string to TRACE.
*		22 Nov 2006 - corrected escape of [
*		20 Oct 2007 - corrected handling of backquotes
****************************************************************************
*/
int FMgetparm( char **buf )
{
	int i;                  /* loop index */
	int j;
	int pstart;             /* starting offset of parameter in the buffer */
	int len = 0;            /* len of token from input stream; length of buffer we are sending via inbuf */
	char	*cp;
	char exbuf[2048];         /* expression return buffer */
	
	*buf = inbuf;                             /* next parm placed in input buf */
	
	len = AFIgettoken( fptr->file, inbuf );   /* get next token from stream */
	 
	
	if( len > 0 )                             /* if not at the end of the line */
	{
		inbuf[len] = EOS;                       /* ensure termination */

		TRACE( 3 ) "getparm: len=%d (%s)\n", len, inbuf );

		switch( *inbuf )     
		{
			case CONTINUE_SYM:                 /* continuation mark? */
				if( trace > 1)
					fprintf( stderr, "getparm: continue symbol\n", len );
				while( AFIgettoken( fptr->file, inbuf ) != 0 );  /* skip to e of buf */
				len = AFIgettoken( fptr->file, inbuf );       /* then get next token */
				break;
   
			case EX_SYM:                          /* expression start symbol? */
				if( len > 1 )                       /* push the remainder of the tok */
					AFIpushtoken( fptr->file, inbuf+1 );
				FMevalex( exbuf );                      /* parse expression */
				strcpy( inbuf, exbuf );
				*buf = inbuf;
				len = strlen( inbuf );
				break;

			case '`':
				TRACE(5) "sussing out back quoted parameter: (%s)\n", inbuf );
         	   		AFIsetflag( fptr->file, AFI_F_WHITE, AFI_SET ); 		/* get white space */
         		 
				i = 0;
				cp = inbuf + 1;			/* skip lead bquote */
				do
				{
					if( i + len >= sizeof( exbuf ) )
					{
						exbuf[50] = 0;
						fprintf( stderr, "buffer overrun detected in getparm: %s...\n", exbuf );
						exit( 1 );
					}

					for( ; *cp && *cp != '`'; cp++ )
					{
						if( *cp == '^'  &&  *(cp+1) == '`' )
							cp++;					/* skip escape and put in bquote as is */
						exbuf[i++] = *cp;	
					}

					exbuf[i] = 0;

					if( *cp == '`' )
						break;

					cp  = inbuf;						/* next token will be here */
				}
	      			while( (len = AFIgettoken( fptr->file, inbuf)) >  0 );


				strcpy( inbuf, exbuf );
				*buf = inbuf;
				len = strlen( inbuf );
				TRACE(2) "getparm returning backquoted parameter: len=%d (%s)\n", len, inbuf );
			
				
       				AFIsetflag( fptr->file, AFI_F_WHITE, AFI_RESET ); 
				break;


			default:                               /* all other tokens */
       				if( len == 1 )
        			{
         				if( *inbuf == ':' )                /* simulate end of buffer */
					{
          					len = 0;                        /* by returning zero length token */
							TRACE(3) "getparm: forced end of parms (:)\n" );
					}
        			}               /* end if token length is one */
				else
				{
					if( (*inbuf == '^' && *(inbuf+1) == ':')  || (*inbuf == '^' && *(inbuf+1) == ':') || (*inbuf == '^' && *(inbuf+1) == '[') )
					{
						len--;
						*buf = inbuf+1;
					}
					else
						if( *inbuf == ':' )
						{
							AFIpushtoken( fptr->file, inbuf+1 );
							len = 0;				/* simulate end of line */
							flags2 |= F2_SMASH;
						}
				}
    		}               
  	}                                    /* end if token len > 0 */
 	else                                   /* end of input line - end of parms */
  		inbuf[0] = EOS;                            /* ensure "empty" buffer */

 	return len;
}                 /* FMgetparm */
