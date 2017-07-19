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
*            This routine must NEVER call FMflush.
*  Parms:    option - TRUE if block structure is completely initialized
*                     (ie a new box not continuation to next column)
*		The remainder of the parms are implemented only by hfm at the 
*		moment and are ignored here. 
*  Returns:  Nothing.
*  Date:     15 October 192
*  Author:   E. Scott Daniels
*  Modified: 26 Oct 1992 - To call flush
*            11 Nov 1992 - Not to call flush
*            17 Nov 1992 - To set lmar & rmar only when option is true
*            23 Apr 1993 - To set box.lmar/rmar on each call to properly
*                          handle new columns where only the lmar value changes
*             7 Apr 1994 - To better display box at top of column
*            11 Arp 1994 - To use FMgetpts routine to allow vert info in inches
*                          and to make margins relative to column.
*	     11 Aug 2001 - To ignore all of the html box options 
**************************************************************************
*/
void FMbxstart( int option, char *colour, int border, int width, char *align )
{
 int len;          /* length of paramters passed in */
 char *buf;        /* pointer to next option to parse */
 int i;            /* loop index */
 int j;

 if( option == FALSE )
  {
   box.topy = cury;                    /* set box top */
   cury += textspace + textsize;       /* skip down a bit */
  }
 else
  box.topy = cury + textspace;     /* set the top of the box to current pos */

 if( option )    /* if true - then setup rest of the block */
  {
	i = 0;
	
	box.lmar = -3; 			   /* default relative positions to column margins */
	box.rmar = cur_col->width + 3;
	box.hconst = FALSE;                   /* default to no hlines */

	while( (len = FMgetparm( &buf )) > 0 ) 	/* snag parameters */
	{
		switch( tolower( *buf ) )
		{
			case 'h':		/* horizontal lines option */
				box.hconst = TRUE;
				break;

			case 'm':		/* margin box rather than column box */
          			box.lmar = (lmar - cur_col->lmar) -3;  /* relative val to col lmar */
          			box.rmar = box.lmar + linelen + 6;     /* relative to col lmar */
				break;
	
			default:			/* assume vert line option if dig, else ignore */
				if( isdigit( *buf ) && i < MAX_VCOL )
					box.vcol[i] = FMgetpts( buf, len );  /* get offset  */
				break;
		}
	}

	box.vcol[i] = -1;    /* mark end of vcol list */
  }                     /* end if option was true */
}                       /* FMbxstart */
