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
*   Modified:  5 May 1992 - To support postscript conversion.
*              3 Dec 1992 - To call add tok to place di token(s) into buffer
*                           so that parens are escaped.
*              12 Jul 1992- To support right justified definition items and
*                           to allow def items to be displayed in an alternate
*                           font.
*              21 Feb 1994- To use dlstack value as shift value for left margin
*                           rather than the old left margin. Corrects multi col
*                           problem.
*               7 Apr 1994- To allow multiple tokens in a def item.
*	       12 Aug 2001- To set ocace after first flush.
*	       10 Feb 2002- To support auto skip option
*		10 Apr 2007 - Fixed bug in assignment of curfont;
*
*   Copyright (c) 1989  E. Scott Daniels. All rights reserved.
***************************************************************************
*/
void FMditem( )
{
 char *buf;           /* pointer at the token */
 char *oldfont;       /* holder of current font when changing font */
 int oldlen;          /* hold current line length */
 int lflags;          /* local flags */
 int len;             /* len of parameter entered */
 char *cfmt = "%c";	/* format strings - defaults if user did not supply */
 char *dfmt = "%d"; 
 char wbuf[25];       /* output buffer for x value when right justifying */

 if( dlstackp < 0 )   /* if no stack pointer then no list in effect */
  {
   FMmsg( E_NO_DEFLIST, NULL );
   return;
  }

 FMflush( );                 		/* put out whats in the buffer */
 if( dlstack[dlstackp].skip )
 {
	AFIpushtoken( fptr->file, "1" );
	FMspace( );
 }

 if( flags2 & F2_DIRIGHT )   /* right justifiy the item? */
  flags2 |= F2_RIGHT;        /* indicate right for flush */

 if( difont != NULL )        /* need to force font change too? */
  {
   oldfont = curfont;        /* save current font */
   curfont = strdup( difont );         /* point at new font string */
   FMfmt_add( );
  }

 lflags = flags;                    /* save state of flags */
 flags &= ~JUSTIFY;                 /* turn off justify for the moment */
 oldlen = linelen;                  /* set line length */
 linelen = dlstack[dlstackp].indent - 3;   /* line length for addtok */
 lmar = lmar - dlstack[dlstackp].indent;   /* set margin for the def term */

 switch( dlstack[dlstackp].anum )
  {
   case DI_ANUMA:                  /* alpha "numbering */
     if( dlstack[dlstackp].fmt )
     	cfmt = dlstack[dlstackp].fmt;
     sprintf( wbuf, cfmt, dlstack[dlstackp].astarta + dlstack[dlstackp].aidx);
     FMaddtok( wbuf, strlen( wbuf ) ); 
     sprintf( wbuf, ".dv _dinum %c : ", dlstack[dlstackp].astarti + dlstack[dlstackp].aidx );
     dlstack[dlstackp].aidx++;
     while( (len = FMgetparm( &buf )) > 0 );         /* skip any parms put in */
     AFIpushtoken( fptr->file, wbuf );               /* define variable */
     break;

   case DI_ANUMI:                  /* integer numbering */
     if( dlstack[dlstackp].fmt )
     	dfmt = dlstack[dlstackp].fmt;
     sprintf( wbuf, dfmt, dlstack[dlstackp].astarti + dlstack[dlstackp].aidx);
     FMaddtok( wbuf, strlen( wbuf ) ); 
     dlstack[dlstackp].aidx++;
     while( (len = FMgetparm( &buf )) > 0 );  /* skip any parms put in */
     break;

   default:
	while( (len = FMgetparm( &buf )) > 0 )   /* add parms to output buffer */
	FMaddtok( buf, len );                   /* display if it fills up */
	if( osize > linelen )
		AFIpushtoken( fptr->file, ".sp 1" );	/* force definition to next line */
	break;
  }

 FMflush( );                 /* flush out the term using the termmar */

 flags2 &= ~F2_RIGHT;        /* ensure right indicator is off */

 if( difont != NULL )        /* if we saved the current font restore it */
  {
   curfont = oldfont;
   FMfmt_add( );
  }

 if( lflags & JUSTIFY )         /* was justify set when we entered? */
  flags |= JUSTIFY;             /* then ensure its on when we leave */
 linelen = oldlen;              /* restore line length */
 lmar += dlstack[dlstackp].indent;     /* restore the current lmar value for text */
 cury -= textsize + textspace;  /* reset so next text line placed at same y */
}                               /* FMditem */
