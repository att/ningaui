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
*   Mnemonic:  FMsetjus
*   Abstract:  This routine sets or resets the justify flag.
*   Parms:     None.
*   Returns:   Nothing.
*   Date:      10 December 1988
*   Author:    E. Scott Daniels
*
***************************************************************************
*/
void FMsetjust( )
{
 char *buf;          /* pointer at the token */
 int len;

 len = FMgetparm( &buf );    /* is there a length? */

 if( len <= 0 )     /* no parameter entered */
  {
   flags = flags | JUSTIFY;       /* turn on the flag by default */
   return;
  }

 switch( buf[1] )           /* see what the user entered */
  {
   case 'n':
   case 'N':                   /* user wants it on */
    flags = flags | JUSTIFY;    /* so do it */
    return;

   default:
    flags = flags & (255-JUSTIFY);       /* turn off the flag by default */
    return;
  }             /* end switch */
}               /* FMsetjust */
