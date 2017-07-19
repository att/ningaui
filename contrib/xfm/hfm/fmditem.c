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
*               6 Dec 1996 - To convert to hfm
*              30 Apr 1997 - To convert to use tables instead of dd/di
*              10 Feb 2002 - To add support for skip and format options
*		14 Sep 2002 - To support colour change for term
*
*   Copyright (c) 1994  E. Scott Daniels. All rights reserved.
***************************************************************************
*/
void FMditem( )
{
 char *buf;           /* pointer at the token */
 int len;             /* len of parameter entered */
 char tbuf[256];      /* buffer to build temp string in */
 char colour[50];
 char *align = " ";
 char *cfmt = "%c";	/* auto numbering default printf format strings */
 char *dfmt = "%d";

 if( dlstackp < 0 )   /* if no stack pointer then no list in effect */
  {
   FMmsg( E_NO_DEFLIST, NULL );
   return;
  }

	if( dlstack[dlstackp].colour )
		sprintf( colour, "color=%s", dlstack[dlstackp].colour );
	else
		colour[0] = 0;


 if( flags2 & F2_DIRIGHT )      /* align term to the right? */
  align="align=right";

 FMflush( );                 /* put out whats in the buffer */
 if( dlstack[dlstackp].skip )
 {
	AFIpushtoken( fptr->file, "1" );
	FMspace( );
 }

 sprintf( tbuf, 
       "</font></td><tr><td %s valign=top width=%d%%><font size=%d %s><b>", 
        align, (dlstack[dlstackp].indent*100)/PG_WIDTH_PTS, textsize, colour );
 strcat( obuf, tbuf );
 optr += strlen( tbuf );
 FMflush( );                 /* put out whats in the buffer */


 switch( dlstack[dlstackp].anum )
  {
   case DI_ANUMA:                  /* alpha "numbering */
	if( dlstack[dlstackp].fmt )
		cfmt = dlstack[dlstackp].fmt;
	sprintf( tbuf, cfmt, dlstack[dlstackp].astarta + dlstack[dlstackp].aidx);
	FMaddtok( tbuf, strlen( tbuf ) );
	if( ++ dlstack[dlstackp].aidx > 26 )
		dlstack[dlstackp].aidx = 0;
	while( (len = FMgetparm( &buf )) > 0 );  /* skip any parms put in */
	break;

   case DI_ANUMI:                  /* integer numbering */
	if( dlstack[dlstackp].fmt )
		dfmt = dlstack[dlstackp].fmt;
	sprintf( tbuf, dfmt, dlstack[dlstackp].astarti + dlstack[dlstackp].aidx);
	FMaddtok( tbuf, strlen( tbuf ) );
	sprintf( tbuf, ".dv _dinum %d : ", dlstack[dlstackp].astarti + dlstack[dlstackp].aidx );
	dlstack[dlstackp].aidx++;
	while( (len = FMgetparm( &buf )) > 0 );  /* skip any parms put in */
	AFIpushtoken( fptr->file, tbuf );
	break;

   default:
     while( (len = FMgetparm( &buf )) > 0 )   /* add parms to output buffer */
      FMaddtok( buf, len );                   /* display if it fills up */
     break;
  }

 FMflush( );                 /* put out whats in the buffer */
 sprintf( tbuf, "</b></font></td><td valign=top><font size=%d %s>", textsize, colour );
 strcat( obuf, tbuf );
 optr += strlen( tbuf );
 FMflush( );                 /* flush out the term using the termmar */
 flags2 |= F2_SETFONT;
}                               /* FMditem */
