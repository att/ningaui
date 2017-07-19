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
*  Mnemonic: FMceject
*  Abstract: This rotine ejects the current column such that the next tokin
*            is placed at the top of the next column on the page. If the
*            current column is the last defined column then pflush is
*            called to flush the page and then the first column is selected.
*            FMflush MUST be called prior to this routine... This routine
*            cannot call FMflush as it is called by FMflush and a loop might
*            occur.
*  Parms:    None.
*  Date:     6 May 1992
*  Returns:  Nothing.
*  Author:   E. Scott Daniels
*
*  Modified: 13 Jul 1994 - To convert to rfm
*             8 Dec 1994 - To write par mark at top only if not in def list.
*             6 Dec 1996 - To convert to hfm (not supported)
****************************************************************************
*/
void FMceject( )
{
}                         /* FMceject */
