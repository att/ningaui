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
**************************************************************************
*
*  TFM
*  Mnemonic: FMbxstart
*  Abstract: This box will setup for a box at the current y position.
*            with the left and right margin values set based on the info
*            in the current column block. If the option is TRUE then this
*            routine assumes that the veritcal column numbers are in the
*            input buffer, and will attempt to read them in and set them
*            up (this is so the routine can be called from columeject.
*            Calling routine is responsible for calling FMflush if necessary.
*            This routine must NEVER call FMflush, or call a routine that
*            calls FMflush.
*  Parms:    option - TRUE if block structure is completely initialized
*                     (ie a new box not continuation to next column)
*                     NOTE: The parameter is not used, but kept as a place
*                            holder as unmodified PSFM routines will call
*                            this routine
*  Returns:  Nothing.
*  Date:     15 October 192
*  Author:   E. Scott Daniels
*  Modified:  1 Jul 1994 - To convert to rtf
*            29 Apr 1997 - To make a one celled table for the box
*            26 Mar 2000 - To add border and width parms
*            23 Mar 2001 - To make inert for TFM; here we go again!
*****************************************************************************
*/
void FMbxstart( int option, char *colour, int border, int width, char *align )
{
 FMignore( );    /* ignore all parameters for the command */
}                       /* FMbxstart */
