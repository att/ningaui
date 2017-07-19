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
*  Mnemonic: FMright
*  Abstract: This routine will put the rest of the line of text entered with
*            this command (.ri) lined up with the right margin.
*  Parms:    None.
*  Returns:  Nothing.
*  Date:     28 July 1994
*  Author:   E. Scott Daniels
*
*            .ri <text to place in doc> <eos>
*
*****************************************************************************
*/
void FMright( )
{
 char *buf;          /* pointer at parameters */
 int len;            /* length of the token */

 FMflush( );                      /* write what ever might be there */

 flags2 |= F2_RIGHT;                      /* make flush right just the text */

 while( (len = FMgetparm( &buf )) > 0 )   /* until all parameters are gone */
  FMaddtok( buf, len );                   /* add it to the output buffer */

 FMflush( );                              /* send user line on the way */

 flags2 &= ~(F2_RIGHT | F2_SAMEY);  /* turn off flags */
}      /* fmcenter */
