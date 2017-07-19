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
*   Mnemonic:  FMtopmar
*   Abstract:  This routine sets up the top margin (topy value)
*   Parms:     None.
*   Returns:   Nothing.
*   Date:      3 December 1988
*   Author:    E. Scott Daniels
*
*   Modified:  25 Mar 1993 - To convert to postscript.
*               7 Apr 1994 - To allow getpts routine to get & convert parm
***************************************************************************
*/
void FMtopmar( )
{
 char *buf;          /* pointer at the token */
 int len;

 if( (len = FMgetparm( &buf )) <= 0 )     /* no parameter entered */
  topy = DEF_TOPY;   /* then set to default */
 else
  {
   topy = FMgetpts( buf, len );  /* convert user input to points */
   /*topy = atoi( buf ); */      /* convert user parameter */
   if( topy > boty - 100 )       /* must have at least 100 points to write */
    topy = boty - 100;
   else
    if( topy <= 0 )              /* must be at least 1 point down */
     topy = 1;
  }                        /* end parameter entered */
}                          /* FMtopmar */
