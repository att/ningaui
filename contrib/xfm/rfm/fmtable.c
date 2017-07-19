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
*  Mnemonic: FMtable
*  Abstract: This routine is called when a table command is encountered.
*            It gets the column sizes from the parameter list and 
*            initialized the table array.
*  Parms:    None.
*  Returns:  Nothing.
*  Date:     18 April 1997
*  Author:   E. Scott Daniels
*  Modified: 15 Feb 1999 - to support w=xx hfm table width
*
*  Command syntax:  .ta [b] [w=xx] <width1> <width2> ... <widthn>
*
*****************************************************************************
*/

void FMtable( )
{
 char *buf;        /* pointer at next input parameter */
 int len;          /* length of token */
 int i = 1;        /* loop index */

 table_stack[ts_index] = malloc( sizeof( struct table_mgt_blk) );
 tableinfo = table_stack[ts_index++]->cells = malloc( (sizeof(int) * MAX_CELLS)+1) ;
 *tableinfo = 0;  /* fake out next bit of original code before nested tables */
 
 if( *tableinfo == 0 )   /* ensure table not already started */
  {
   flags2 &= ~F2_BORDERS;     /* default to no borders */

   while( (len = FMgetparm( &buf) ) != 0  )
    switch( *buf )
     {
      case 'a':          /* hfm options we simply must ignore */
      case 'c':
      case 'n':
      case 'v':
      case 'p':
      case 's':
          break;

      case 'b':
          flags2 |= F2_BORDERS;   /* turn on borders */
          break;

      case 'w':          /* table width parm (hfm only for now */
          break;

      default:
          if( i < MAX_CELLS )
           {
            tableinfo[i] = FMgetpts( buf, len );
            i++;                                 /* cell counter */
           }
          break;
     }
  
   tableinfo[0] = i;         /* save number of cells in each row */
  
   AFIwrite( ofile, "{" );   /* enclose the table in braces */
   FMrow( FIRST );           /* start first row of table */
  }
}                /* fmtable */
