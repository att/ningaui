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
******************************************************************************
*
*  Mnemonic: FMrunset
*  Abstract: This routine is responsible for outputting the necessary commands
*            to get the header, footer and page number going.
*            This routine should be called any time a header footer or page
*            number option is changed. (Unfortunately the page numbering is
*            left to be done by the rtf device and thus the actual number
*            cannot be controlled by rfm.)
*  Parms:    none.
*  Returns:  Nothing.
*  Date:     11 July 1994
*  Author:   E. Scott Daniels
*
*  Modified: 12 Jul 1994 - To allow page number and footer.
*             6 Dec 1996 - To convert for hfm (not supported)
******************************************************************************
*/
void FMrunset( )
{
}                /* FMrunset */
