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
* --------------------------------------------------------------------------
*  Mnemonic: fmcell
*  Abstract: Start  the next cell
*  Date:     22 Feb 1999 - broken out of cmd 
*  Author:   E. Scott Daniels
* 
*  .cl [c=bgcolour] [t=fgcolour] [s=span-cols] [a=align-type]
* --------------------------------------------------------------------------
*/

void FMcell( int parms )
{
 char buf[256];
 char buf2[256];
 char colour[50];        /* buffer to build bgcolor= string in */
 char span[50];          /* buffer to build colspan= string in */
 char rspan[50];          /* buffer to build rowspan= string in */
 char align[50];         /* buffer for align= string */
 char valign[50];         /* buffer for align= string */
 int len;
 char *ptr;

 span[0] = 0;
 align[0] = 0;
 buf[0] = 0;
 buf2[0] = 0;
 rspan[0] = 0;
 valign[0] = 0;
 sprintf( valign, "valign=top" );

 if( ! table_stack || ! ts_index )
{
 	while( parms && (len = FMgetparm( &ptr )) != 0 );
	return;
}

 if( table_stack[ts_index-1]->bgcolour  )
  sprintf( colour, "bgcolor=%s", table_stack[ts_index-1]->bgcolour );
 else
  colour[0] = 0;

 colour[0] = 0;
 FMflush( );                       /* flush what was in the buffer first */
 
 if( ++curcell > MAX_CELLS )
  curcell = 1;

 while( parms && (len = FMgetparm( &ptr )) != 0 )
  switch( *ptr )
   {
    case 'a':
      sprintf( align, "align=%s", ptr + 2 );
      break;

    case 'c':   
      sprintf( colour, "bgcolor=%s", ptr + 2 );
      break;

    case 'r':
	sprintf( rspan, "rowspan=%s", ptr + 2 );
	break;

    case 's':
      sprintf( span, "colspan=%s", ptr + 2 );
      break;

    case 't':      /* really does not make sense to support this */
      break;

    case 'v':
      sprintf( valign, "valign=%s", ptr + 2 );
      break;

    default: 
      break;
   }

 sprintf( buf, "</td><td %s %s %s %s %s", colour, span, valign, align, rspan );

 if( tableinfo &&  tableinfo[curcell] )
  sprintf( buf2, " width=%d%%>", (tableinfo[curcell]*100)/PG_WIDTH_PTS );
 else
  strcpy( buf2, ">" );

 strcat( buf, buf2 );
  
 AFIwrite( ofile, buf );
 flags2 |= F2_SETFONT;

}
