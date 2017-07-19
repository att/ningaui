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
* TFM 
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
*              6 Dec 1996 - To convert for hfm (line length based only on
*                           the number of characters in the buffer as
*                           display utility will format, punctuation space
*                           is ignored as browser will provide.
*             21 Mar 2001 - Ironicly back to whence we began - tfm
*             23 Apr 2001 - To add support for .tu command
*************************************************************************
*/
void FMaddtok( buf, len )
 char *buf;
 int len;
{
 int remain;          /* calculated space remaining before the right margin */
 int i;               /* loop index */
 int toksize;         /* size of token (#characters) */

 flags2 |= F2_OK2JUST;

 words++;             /* increase the number of words in the document */

 if( len > ((linelen/7) - optr) )         /* room for it? */
   FMflush( );

 for( i = 0; i < len && xlate_u; xlate_u-- )          /* translate token if necessary */
  {
    buf[i] = toupper( buf[i] );
    i++;
  }

 xlate_u = 0;                         /* in case user asked for more than token size */

 for( i = 0; i < len; i++, optr++ )     /* copy token to output buffer */
  {                                     /* escaping special html characters */
   switch( buf[i] )
    {
#ifdef NEVER_ME
     case '>':
      obuf[optr] = EOS;                 /* terminate for strcat */
      strcat( obuf, "&gt;" );           /* copy in the character */
      optr += 3;                        /* incr past the html escape string */
      osize += 3;                       /* incr number of chars in output */
      break;

     case '<':
      obuf[optr] = EOS;                 /* terminate for strcat */
      strcat( obuf, "&lt;" );           /* copy in the character */
      optr += 3;                        /* incr past the html escape string */
      osize += 3;                       /* incr number of chars in output */
      break;

     case '&':
      obuf[optr] = EOS;                 /* terminate for strcat */
      strcat( obuf, "&amp;" );          /* copy in the character */
      optr += 4;                        /* incr past the html escape string */
      osize += 4;                       /* incr number of chars in output */
      break;
#endif

     case '^':                  /* allow user to escape ><& characters */
      i++;                  /* point at next character and drop into insert */

     default:                 /* not a special character - just copy in */
      obuf[optr] = buf[i];
      obuf[optr+1] = EOS;      /* just incase one of the above encountered */
      break;
    }                         /* end switch */
  }                            /* end copy of token */

 if( ! (flags2 & F2_QUOTED) )
  {
   obuf[optr]= BLANK;            /* terminate token with a blank */
   optr++;                       /* set for location of next token */
  }
 obuf[optr] = EOS;             /* terminate buffer incase we flush */

 
 flags2 &= ~F2_OK2JUST;        /* justify only on buffers dumped from here */
}                              /* FMaddtok */

