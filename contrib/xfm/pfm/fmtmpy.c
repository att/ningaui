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
*  Mnemonic: FMtmpy
*  Abstract: This routine is called to process the .tt and .tb commands which
*            set the topy/boty values to the current y position allowing
*            some flexability in including multi column artwork or sidebars.
*            if the real value (rtopy rboty) is 0 then the current value
*            is saved and the current y position is made the temp max value.
*            If the real value is not 0 then the top/boty values are restored.
*            A parameter may be supplied indicating when the value is to be
*            restored (after n column ejects) and if omited defaults to at the
*            next page eject.
*  Parms:    cmd - Command that caused this invocation.
*  Returns:  Nothing.
*  Date:     7 April 1994
*  Author:   E. Scott Daniels
*
*	.tt [value]
*****************************************************************************
*/
void FMtmpy( cmd )
{
 char *buf;         /* pointer to the parameter */
 int len;

 switch( cmd )
  {
   case C_TMPTOP:   /* set temp top y value */
    rtopcount = -1;          /* force reset at top of page */
    if( rtopy == 0 )         /* if tmp not already set */
      rtopy = topy;            /* save current topy in real topy */

    topy = cury;             /* set topy y here as the default */
    if( (len = FMgetparm( &buf )) > 0 )
	topy = FMgetpts( buf, len );
    break;

   default:
     break;
  }
}                    /* FMtmpy */

