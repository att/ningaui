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
*  Mnemonic: FMendtable
*  Abstract: This routine is called when the .et command is encountered 
*            and terminates the table.
*  Parms:    None.
*  Returns:  Nothing.
*  Date:     18 April 1997
*  Author:   E. Scott Daniels
*  
*  command syntax: .et
*
***************************************************************************
*/
void FMendtable( )
{
 AFIwrite( ofile, "\\cell \\intbl \\row \\pard}" );

 if( ts_index )
   ts_index--; 
 if( table_stack[ts_index] )
  {
   free( table_stack[ts_index]->cells );
   free( table_stack[ts_index] );
   table_stack[ts_index] = 0;
   if( ts_index )
    tableinfo =  table_stack[ts_index - 1]->cells;
   else
    tableinfo = 0;
  }

 /* tableinfo[0] = 0; */

}      /* FMendtable */
