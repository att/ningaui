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


#include "fmcmds.h"
#include "fmstruct.h"              /* structure definitions */
#include "fmproto.h"

/*
*****************************************************************************
*
*   Mnemonic: FMnofmt
*   Abstract: This routine is called when the noformat flag is set. It
*             reads in lines from the input file and if there is not a
*             command in the first column then it places the line of
*             text as is out to the page buffer. If a command is on the
*             input line control is returned to the caller to process the
*             command.
*   Parms:    None.
*   Returns:  Nothing.
*   Date:     8 December 1988
*   Author:   E. Scott Daniels
*
*   Modified:  7 Jul 1994 - To convert to rfm
*              4 Oct 1994 - To reduce amount skipped in y direction.
*              6 Dec 1996 - To convert for hfm
*              4 Apr 1997 - To use the new tokenizer in AFIleio!
*             15 Apr 1997 - To return if vardelim is in first col too
*******************************************************************************
*/
void FMnofmt( )
{
 char *buf;             /* work buffer */
 int status;            /* status of the read */
 int i;                 /* loop index */

 status = FMread( inbuf );        /* get the next buffer */

 while( status >= 0  &&  inbuf[0] != CMDSYM && *inbuf != vardelim ) 
  {
   for( i = 0; i < MAX_READ-1 && inbuf[i] != EOS; i++, optr++ )
    {
     switch( inbuf[i] )          /* properly escape html special chars */
      {
       case '>':
        obuf[optr] = EOS;                 /* terminate for strcat */
        strcat( obuf, "&gt;" );           /* copy in the character */
        optr += 3;                        /* incr past the html esc string */
        osize += 3;                       /* incr number of chars in output */
        break;

       case '<':
        obuf[optr] = EOS;                 /* terminate for strcat */
        strcat( obuf, "&lt;" );           /* copy in the character */
        optr += 3;                        /* incr past the html esc string */
        osize += 3;                       /* incr number of chars in output */
        break;

       case '&':
        obuf[optr] = EOS;                 /* terminate for strcat */
        strcat( obuf, "&amp;" );          /* copy in the character */
        optr += 4;                        /* incr past the html esce string */
        osize += 4;                       /* incr number of chars in output */
        break;

       default:                 /* not a special character - just copy in */
        obuf[optr] = inbuf[i];
        obuf[optr+1] = EOS;     /* terminate incase of strcat on next loop */
        break;
      }
    }                          /* end while stuff in input buffer to copy */

   if( optr == 0 )             /* if this was a blank line */
    obuf[optr++] = ' ';        /* give flush a blank to write */
   obuf[optr] = EOS;           /* terminate buffer for flush */

   FMflush( );                 /* send the line on its way */
   status = FMread( inbuf );   /* get the next line and loop back */
  }                            /* end while */

 AFIpushtoken( fptr->file, inbuf );   /* put command back into input stack */
 iptr = optr = 0;              /* return pointing at beginning */
}                              /* FMnofmt */
