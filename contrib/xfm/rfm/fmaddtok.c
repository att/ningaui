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
*   Modified:  7 Jul 1994 - To convert to rfm
*              8 Apr 1997 - To properly recognize the ^ token 
*             23 Apr 2001 - To add support for .tu command
*************************************************************************
*/
void FMaddtok( buf, len )
 char *buf;
 int len;
{
 int remain;          /* calculated space remaining before the right margin */
 int i;               /* loop index */
 int toksize;         /* size of token in points */

 words++;             /* increase the number of words in the document */

 if( optr == 0 )       /* if starting at beginning */
  osize = 0;           /* reset current line size */

 remain = linelen - osize;    /* get space remaining for insertion */

 if( (toksize = FMtoksize( buf, len )) > remain )  /* room for it? */
  {                                    /* no - put out current buffer */
   if( flags & JUSTIFY )
    FMjustify( );            /* sendout in justification mode */
   else
    FMflush( );        /* if not justify - send out using flush  */
   osize = 0;          /* new buffer has no size yet */
  }

 osize += toksize;         /* add current token to current length */

 for( i = 0; i < len && xlate_u; xlate_u-- )          /* translate token if necessary */
  {
    buf[i] = toupper( buf[i] );
    i++;
  }

 xlate_u = 0;                        /* in case user asked for more than token size */

 for( i = 0; i < len; i++, optr++ )     /* copy token to output buffer */
  {
   if( buf[i] == '^' )                  /* output next character as is */
    i++;
   else
    if( buf[i] == '{' || buf[i] == '}' || buf[i] == '\\' ) /* need to escape */
     {
      obuf[optr] = '\\';       /* put in the escape character */
      optr++;                  /* and bump up the pointer */
     }
   obuf[optr] = buf[i];         /* copy the next char to the output buffer */
  }    /* end copy of tokin */

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

