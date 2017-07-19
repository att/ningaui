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
*   Mnemonic:  FMpgnum
*   Abstract:  This routine sets the page number flag as requested by the
*              user.
*   Parms:     None.
*   Returns:   Nothing.
*   Date:      3 December 1988
*   Author:    E. Scott Daniels
*
***************************************************************************
*/
void FMpgnum( )
{
 char *buf;          /* pointer at the token */
 int len;            /* length of parameter entered */

 if( (len = FMgetparm( &buf )) <= 0 )  /* if missing assume on @ current page */
  {
   flags = flags | PAGE_NUM;
   return;
  }

 if( strncmp( "off", buf, 3 ) == 0 )
   flags = flags & (255 - PAGE_NUM);       /* turn off if requested */
 else
  {
   flags = flags | PAGE_NUM;
   if( strncmp( "on", buf, 2 ) == 0 )    /* assume # will be next */
    len = FMgetparm( &buf );             /* see if # was entered */
   if( len > 0 )                  /* if there is a number there */
    page = atoi( buf );           /* convert next page number */
  }
}               /* FMpgnum */
