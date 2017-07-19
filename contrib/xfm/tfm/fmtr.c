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
---------------------------------------------------------------------------
 Mnemonic: fmtr
 Abstract: start a new row in the current table
 The n option prevents cell from being called.
 syntax:   .tr [n] [c=bgcolour] [a=alignval] [v=valignvalue]
---------------------------------------------------------------------------
*/
void FMtr( )
 {
   char *ptr;             /* pointer at parms */
   int len;
   int do_cell = 1;       /* turned off by n option */
   char colour[50];
   char align[50];
   char valign[50];
   char obuf[256];

   colour[0] = 0;
   align[0] = 0;
   valign[0] = 0;

   FMflush( );                               /* empty the cache */
   curcell = 0;

   while( (len = FMgetparm( &ptr )) != 0 )
    {
     switch( *ptr ) 
      {
       case 'a':
         sprintf( align, "align=%s", ptr + 2 );
         break;

       case 'c':
         sprintf( colour, "bgcolor=%s", ptr + 2 );
         break;

       case 'n':
         do_cell = 0;
         break;

       case 'v':
         sprintf( valign, "valign=%s", ptr + 2 );
         break;


       default: 
         break;
      }
    }

   sprintf( obuf, "</td></tr><tr %s %s %s>", colour, align, valign );
   AFIwrite( ofile, obuf );

   if( do_cell )
    FMcell( 0 );                               /* start 1st cell */
   flags2 |= F2_SETFONT;           /* need to change font on next write */
 }
