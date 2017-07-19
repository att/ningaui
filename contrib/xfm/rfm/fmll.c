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
*   Mnemonic:  FMll
*   Abstract:  This routine resets the line length.
*   Parms:     None.
*   Returns:   Nothing.
*   Date:      17 November 1988
*   Author:    E. Scott Daniels
*
*   .ll  n | +n | -n  (default if n omitted is 65)
*
*   Modified:  1 Jul 1994 - To convert to rfm
***************************************************************************
*/
void FMll( )
{
 char *buf;          /* pointer at the token */
 char obuf[25];
 int len;

 if( (len = FMgetparm( &buf )) > 0 )  /* if user supplied parameter */
  {
   len = FMgetpts( buf, len );          /* convert it to points */

   if( buf[0] == '+' || buf[0] == '-' )
    len = len + linelen;          /* user wants parameter added/subed */

   if( len > cur_col->width || len <= 0 )   /* if out of range */
    FMmsg( W_LL_TOOBIG, buf );             /* let user know */
   else
    {
     linelen = len;                          /* set the line length */
     len = cur_col->width - (linelen + (lmar-cur_col->lmar));
    }
  }
}               /* FMll */
