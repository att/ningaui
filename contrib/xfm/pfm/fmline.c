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
*****************************************************************************
*
*  Mnemonic: FMline
*  Abstract: This routine is responsible for putting a line into the output
*            file.
*  Parms:    None.
*  Returns:  Nothing.
*  Date:     1 November 1992
*  Author:   E. Scott Daniels
*
*  Modified: 10 Dec 1992 - To use AFI routines for ansi compatability
*            11 Apr 1994 - To check for box to see what margins to use.
*****************************************************************************
*/
void FMline( )
{
 char out[100];  /* output buffer */
 int len;        /* lenght of output string */

 FMflush( );     /* put out what ever is currently cached */

 if( flags2 & F2_BOX )          /* if in box use box margins */
  sprintf( out,
      "gsave %d setlinewidth %d -%d moveto %d -%d lineto stroke grestore\n",
                linesize, cur_col->lmar + box.lmar, cury+4,
                          cur_col->lmar + box.rmar, cury+4 );
 else
  sprintf( out,
      "gsave %d setlinewidth %d -%d moveto %d -%d lineto stroke grestore\n",
                linesize, cur_col->lmar, cury+4,
                          cur_col->lmar + cur_col->width, cury+4 );
 AFIwrite( ofile, out );   /* write the command to draw the line out */

 cury += 8 + linesize;   /* bump the current spot up some */
}           /* FMline */
