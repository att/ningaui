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
****************************************************************************
*
*  Mnemonic: FMpara
*  Abstract: This routine will output the RTF instructions necessary to
*            start a new "paragraph" using the current left margin value
*            and assigning the field indent value that is passed in. A
*            tab stop at the left margin is also set.
*            This routine will make all necessary conversions to points
*  Parms:    fi - Field indention value (points)
*            opt- True if we end the current paragraph first
*  Returns:  nothing/
*  Author:   E. Scott Daniels
*
*  Modified: 19 Jul 1994 - To allow list items in boxes
****************************************************************************
*/
void FMpara( fi, opt )
 int fi;
 int opt;
{
 char buf[81];        /* output build buffer */
 int right;           /* value for right shift */

 right = cur_col->width - ((lmar-cur_col->lmar) + linelen);

 if( opt )
  AFIwrite( ofile, "\\par" );     /* terminate current paragraph */

 sprintf( buf, "\\pard\\qj\\li%d\\fi%d\\tx%d\\ri%d",
           lmar * 20, fi * 20, lmar * 20, right * 20 );

 AFIwrite( ofile, buf );

 if( flags2 & F2_BOX )
	FMbxstart( FALSE, 0, 0, 0, 0 );

 rflags |= RF_PAR;
}                   /* FMpara */
