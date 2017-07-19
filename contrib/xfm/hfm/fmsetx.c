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
*  Mnemonic: FMsetx
*  Abstract: This routine is responsible for adjusting the current x position
*            on the output page. The value entered may be specified in pts or
*            inches. A + or - prefix to the value indicates a relitive move,
*            and a value without + or - indicates an absolute position in
*            relation to the current column's left margin value.
*            NOTE: In RFM the +/- value cannot be supportd. The value will
*                  be treated as an absolute value.
*  Params:   None.
*  Returns:  Nothing.
*  Date:     3 December 1992
*  Author:   E. Scott Daniels
*
*  Modified: 13 Jul 1994 - To convert to rfm.
*             6 Dec 1996 - To convert for hfm (not supported)
******************************************************************************
*/
void FMsetx( )
{
 char *buf;            /* pointer at the parameter user has entered */

 FMgetparm( &buf );          /* clear the x value from command */
}                                 /* FMsetx */
