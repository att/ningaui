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
*  Parms:    parms are mostly for hfm, but must be accepted and ignored in 
*		other flavours 
*
*  Returns:  Nothing.
*  Date:     15 October 192
*  Author:   E. Scott Daniels
*  Modified:  1 July 1994 - To convert to rtf
*****************************************************************************
*/
/*void FMbxstart( option )*/
void FMbxstart( int option, char *colour, int border, int width, char *align )
{
 int len;          /* length of paramters passed in */
 char *buf;        /* pointer to next option to parse */
 int i;            /* loop index */
 int j;

 if( cury == topy )              /* if this is the first line */
  {
   AFIwrite( ofile, "\\par" );   /* space down some */
   cury += textsize + textspace;
  }

  AFIwrite( ofile, "\\box\\brdrs\\brdrw20\\brsp100" );   /* start box */

 if( option )                             /* if not continuing box */
  while( FMgetparm( &buf ) > OK );        /* strip remainging parms */
}                       /* FMbxstart */
