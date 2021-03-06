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
******************************************************************************
*
*  Menmonic: Fmsetmar
*  Abstract: This routine will set the current margin values based on the
*            current column setting. This routine is specific to RFM only.
*  Parms:    None.
*  Returns:  Nothing.
*  Date:     6 July 1994
*  Author:   E. Scott Daniels
*
*******************************************************************************
*/
void FMsetmar( )
{
 char wbuf[80];          /* ouput work buffer */
 int dist;               /* distance to bring margin in from edge */

 if( firstcol->next != NULL )
  dist = 18;                   /* default when multi column mode */
 else
  dist = 594 - (cur_col->lmar + cur_col->width);   /* width in points */

 if( dist <= 17 )
  dist = 18;        /* set a minimum inset of .25i */

 sprintf( wbuf, "\\margr%d", dist * 20 );   /* convert to rtf units */
 AFIwrite( ofile, wbuf );                   /* and adjust the margin */
}   /* FMsetmar */
