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
*   Mnemonic: FMheader
*   Abstract: This routine is responsible for putting a paragraph header
*             into the output buffer based on the information in the
*             header options buffer that is passed in.
*   Parms:    hptr - Pointer to header information we will use
*   Returns:  Nothing.
*   Date:     1 December 1988
*   Author:   E. Scott Daniels
*
*   Modified: 30 Jun 1994 - Conversion to rfm (not much changed)
*             15 Dec 1994 - To seperate skip before/after numbers.
**************************************************************************
*/
void FMheader( hptr )
 struct header_blk *hptr;
{
 int len;            /* len of parm token */
 int j;              /* index variables */
 int i;
 char buf[10];       /* buffer to build paragraph number in */
 char *ptr;          /* pointer to header string */
 int oldlmar;        /* temp storage for left mar value */
 int oldsize;        /* temp storage for text size value */
 int oldlen;         /* temp storage for old line length */
 char *oldfont;      /* pointer to old font sring */
 int  skip;          /* number of lines to skip before continuing */

 FMflush( );          /* flush the current line */

 if( (rflags & RF_PAR) == 0 )  /* need to end previous section properly? */
  AFIwrite( ofile, "\\par" );

 if( (hptr->flags & HEJECTC) && cury != topy ) /* if coleject and not there */
   FMceject( 1 );
 else
  if( (hptr->flags & HEJECTP) &&
      (cur_col != firstcol || topy != cury) )
   FMpflush( );       /* HEJECT flag indicates that a new page is needed */
  else
   FMccol( 5);            /* ensure 5 lines remain on page before header */


 if( cury != topy  )                      /* if not at beginning of column */
  {
   skip = hptr->skip;
   if( skip >= 10 )                        /* skp before is 10s digit if > 10 */
    skip /= 10;                            /* so shift it */
   skip = skip >= 0 ? skip : 0;            /* min skip is 0 */

   cury += (textsize + textspace) * skip;  /* skip before */
   for( i = 0; i < skip; i++ )
    AFIwrite( ofile, "\\par" );               /* skip lines before */
  }
 else                              /* at the top */
  if( rhead != NULL )              /* and there is a header defined */
   {
    AFIwrite( ofile, "\\par" );    /* insert a blank line */
    cury += textsize + textspace;  /* and indicate we have done this */
   }

 pnum[(hptr->level)-1] += 1;      /* increase the paragraph number */

 if( hptr->level == 1 && (flags & PARA_NUM) )
  fig = 1;                          /* restart figure numbers */

 for( i = hptr->level; i < MAX_HLEVELS; i++ )
  pnum[i] = 0;                       /* zero out all lower p numbers */

 if( flags & PARA_NUM )          /* number the paragraph if necessary */
  {
   sprintf( buf, "%d.%d.%d.%d", pnum[0], pnum[1], pnum[2], pnum[3] );
   for( i = 0, j = 0; j < hptr->level; j++, i++ )
    {
     for( ; buf[i] != '.' && buf[i] != EOS; i++, optr++ )
      obuf[optr] = buf[i];          /* copy in the needed part of the number */
     if( buf[i] != EOS )
      obuf[optr++] = '.';          /* put in the dot */
    }                             /* end for j */
   obuf[optr++] = BLANK;          /* seperate with a blank */
  }

 while( (len = FMgetparm( &ptr )) > 0 )    /* get next token in string */
  {
   for( i = 0; i < len; i++, optr++ )
    if( hptr->flags & HTOUPPER )       /* xlate to upper flag for this header */
     obuf[optr] = toupper( ptr[i] );
    else
     obuf[optr] = ptr[i];           /* copy in the buffer */
   obuf[optr++] = BLANK;            /* add an additional blank */
  }
 obuf[optr] = EOS;                  /* terminate the header string */

/* the call to FMtoc must be made while the header is in obuf */
 if( hptr->flags & HTOC )         /* put this in table of contents? */
  FMtoc( hptr->level );           /* put entry in the toc file */

 oldlen = linelen;                /* save the line length */
 oldlmar = lmar;                  /* save left margin */
 oldfont = curfont;               /* save current text font */
 oldsize = textsize;              /* save old text size */

 if( hptr->level < 4 )            /* unindent if level is less than 4 */
  {
   linelen += (lmar - hlmar) + hptr->hmoffset;   /* adujst line len */
   lmar = hlmar + hptr->hmoffset; /* set lmar to hmarg + the offset for level*/
  }
 textsize = hptr->size;           /* set text size to header size for flush */
 curfont = hptr->font;            /* set font to header font for flush */

 FMpara( 0, FALSE );              /* position for header text */
 flags2 |= F2_SETFONT;            /* have flush set the font */
 FMflush( );                      /* writeout the header line */

 linelen = oldlen;                /* restore line length */
 lmar = oldlmar;                  /* restore left margin value */
 textsize = oldsize;              /* restore text size */
 curfont = oldfont;               /* restore font too */

 flags2 |= F2_SETFONT;      /* next flush will also have to set the font */

 for( optr = 0, i = 0; i < hptr->indent; i++, optr++ )   /* indent */
  obuf[optr] = BLANK;
 obuf[optr] = EOS;                          /* terminate incase flush called */
 osize = FMtoksize( obuf, hptr->indent );   /* set size of blanks */

 if( hptr->level < 4 )      /* indent and skip lines if not h4 */
  {
   skip = hptr->skip;
   if( skip > 10 )                           /* 1s digit is skip after value */
    skip -= (skip / 10) * 10;                /* strip 10s digit */
   else
    skip /= 2;                               /* default back to old way */
   for( j = 0; j <= skip; j++ )
    AFIwrite( ofile, "\\par" );              /* rtf "skip" line cmds to file */
   cury += (textsize+textspace) * skip;      /* adjust y value */
   FMpara( 0, FALSE );
  }
}                    /* FMheader */
