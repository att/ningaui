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
*   Mnemonic:  FMcd
*   Abstract:  This routine parses the parameters on the .cd command and
*              builds the required column blocks.
*   Parms:     None.
*   Returns:   Nothing.
*   Date:      1 December 1988
*   Author:    E. Scott Daniels
*
*   .cd n w i=value[i|p] g=value[i|p]
*           n - The number of columns     (required)
*           w - The width of each column  (optional)
*           i=- The indention of col 1 from edge of page (optional)
*           g=- The gutter space between each column (optional)
*
*   Modified: 13 Jul 1994 - To convert to rfm
*              9 Sep 1994 - To check first parameter properly
*              7 Dec 1996 - To convert to hfm - not supported
***************************************************************************
*/
void FMcd( )
{
 char *buf;                 /* pointer at the token */

 while( FMgetparm( &buf ) != 0 );    /* trash any parameters */

 return;
}               /* FMcd */
