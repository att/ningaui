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
***************************************************************************
*
*  Mnemonic: FMrow
*  Abstract: This routine will start a new row (either first or middle) 
*            in an existing table.
*  Parms:    Option: The constant FIRST or MIDDLE.
*  Returns:  Nothing.
*  Date:     18 April 1997
*  Author:   E. Scott Daniels
*
*  command syntax:  .tr
*
*
*  Modified: 5 Jun 1997 - To set par flag and update cury for each row
***************************************************************************
*/
void FMrow( int option )
{
 char obuf[256];       /* buffer to build output in */
 int i;                /* loop index */ 
 int pts = 0;          /* points before the current column in table */
 
 if( *tableinfo != 0 )   /* ensure table is started */
  {
   if( option == MIDDLE )    /* middle row of table, need to end last cell */
    {
     AFIwrite( ofile, "\\cell \\pard \\intbl \\row" );
    }
  
   sprintf( obuf, "\\pnindent100 \\trowd \\trgraph144 \\trleft%d", lmar * 20 );
   AFIwrite( ofile, obuf );
  
   pts = lmar;      /* indent the box to the left margin */

   for( i = 1; i < *tableinfo; i++ )   /* sart the next row */ 
    {
     pts += tableinfo[i];
     if( flags2 & F2_BORDERS )
      {
       AFIwrite( ofile, "\\clbrdrt\\brdrs\\brdrw15" );   /* turn on borders */
       AFIwrite( ofile, "\\clbrdrl\\brdrs\\brdrw15" );   /* if the flag is */
       AFIwrite( ofile, "\\clbrdrb\\brdrs\\brdrw15" );   /* set in table */
       AFIwrite( ofile, "\\clbrdrr\\brdrs\\brdrw15" ); 
      }

     sprintf( obuf, "\\cellx%d",  pts * 20 );           /* cvt pts to RTFUs */
     AFIwrite( ofile, obuf );
    }
   
   AFIwrite( ofile, "\\pard \\intbl" );
   rflags |= RF_PAR;                     /* indicate we have done this */

   cury += (textsize + textspace);   /* calc estimated space for row */
  }
} 
