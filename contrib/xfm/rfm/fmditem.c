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
*   Mnemonic:  FMditem
*   Abstract:  This routine handles a definition item (.di) command.
*              The left margin is moved left to the start of the term area,
*              and the line length is set to the term size (points). Then
*              addtoken is called to put in the tokens, moving to the next
*              line if necessary. We fake it into thinking that the term
*              area is all that we can write to by adjusting the line len.
*              This routine will process all the tokens on the .di line or
*              until the colon (:) stop token is encountered.
*   Parms:     None.
*   Returns:   Nothing.
*   Date:      1 January 1989 (Happy New Year!)
*   Author:    E. Scott Daniels
*   Modified:   1 Jul 1994 - To convert to rfm.
*
*   Copyright (c) 1994  E. Scott Daniels. All rights reserved.
***************************************************************************
*/
void FMditem( )
{
 char *buf;           /* pointer at the token */
 char *oldfont;       /* holder of current font when changing font */
 int oldlen;          /* hold current line length */
 int lflags;          /* local flags */
 int len;             /* len of parameter entered */
 char wbuf[25];       /* output buffer for x value when right justifying */

 if( dlstackp < 0 )   /* if no stack pointer then no list in effect */
  {
   FMmsg( E_NO_DEFLIST, NULL );
   return;
  }

 FMflush( );                 /* put out whats in the buffer */

 if( flags2 & F2_DIRIGHT )   /* right justifiy the item? */
  flags2 |= F2_RIGHT;        /* indicate right for flush */

 if( difont != NULL )        /* need to force font change too? */
  {
   oldfont = curfont;        /* save current font */
   curfont = difont;         /* point at new font string */
   flags2 |= F2_SETFONT;     /* force flush to set the font first */
  }

 lflags = flags;                    /* save state of flags */
 flags &= ~JUSTIFY;                 /* turn off justify for the moment */
 oldlen = linelen;                  /* set line length */

 if( rflags & RF_PAR )
  FMpara( (dlstack[dlstackp].indent) * (-1), FALSE );  /* setup for next paragraph */
 else
  FMpara( (dlstack[dlstackp].indent) * (-1), TRUE );  /* setup for next paragraph */

 switch( dlstack[dlstackp].anum )
  {
   case DI_ANUMA:                  /* alpha "numbering */
     sprintf( wbuf, "%c", dlstack[dlstackp].astarta + dlstack[dlstackp].aidx);
     FMaddtok( wbuf, strlen( wbuf ) ); 
     sprintf( wbuf, ".dv _dinum %d : ", dlstack[dlstackp].astarti + dlstack[dlstackp].aidx );
     dlstack[dlstackp].aidx++;
     while( (len = FMgetparm( &buf )) > 0 );         /* skip any parms put in */
     AFIpushtoken( fptr->file, wbuf );               /* define variable */
     break;

   case DI_ANUMI:                  /* integer numbering */
     sprintf( wbuf, "%d", dlstack[dlstackp].astarti + dlstack[dlstackp].aidx);
     FMaddtok( wbuf, strlen( wbuf ) ); 
     dlstack[dlstackp].aidx++;
     while( (len = FMgetparm( &buf )) > 0 );  /* skip any parms put in */
     break;

   default:
     while( (len = FMgetparm( &buf )) > 0 )   /* add parms to output buffer */
      FMaddtok( buf, len );                   /* display if it fills up */
     break;
  }

 FMflush( );                 /* flush out the term using the termmar */

 AFIwrite( ofile, "\\tab" );  /* force tab to the right margin */

 flags2 &= ~F2_RIGHT;        /* ensure right indicator is off */

 if( difont != NULL )        /* if we saved the current font restore it */
  {
   curfont = oldfont;
   flags2 |= F2_SETFONT;     /* force flush to reset it next time */
  }

 if( lflags & JUSTIFY )         /* was justify set when we entered? */
  flags |= JUSTIFY;             /* then ensure its on when we leave */
 cury -= textsize + textspace;  /* reset so next text line placed at same y */
}                               /* FMditem */
