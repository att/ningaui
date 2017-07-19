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
*************************************************************************
*
*   Mnemonic: FMformat
*   Abstract: This routine handles the .FO command that the user may
*             enter to signal a change in the automatic formatting
*             done. Command format is .FO {ON | OFF}. ON is the default if
*             the parameter is omitted.
*   Parms:    None.
*   Returns:  Nothing.
*   Date:     8 December 1988
*   Author:   E. Scott Daniels
*   Modified:  5 May 1992 - To support postscript conversion.
*             27 Nov 1992 - To support asis flag
*             28 Jan 1993 - To ensure parm before conversion to uppercase
*
***************************************************************************
*/
void FMformat( )
{
 int i;              /* lengnth of parameter entered on the command line */
 char *buf;          /* pointer to the parameter entered */

 i = FMgetparm( &buf );        /* any parms entered ? */

 if( (i == 0)  ||  (tolower( buf[1] ) == 'n') )    /* if omitted or on */
  {
   flags = flags & (255-NOFORMAT);      /* turn off noformat flag */
   flags2 &= ~F2_ASIS;                  /* turn off asis flag */
   FMflush( );                          /* flush last unformatted line */
  }
 else
  {                  /* assume turning format off */
   if( (flags & NOFORMAT) == 0  &&  (optr != 0) )
    FMflush( );                /* flush last formatted line before command */
   flags = flags | NOFORMAT;     /* turn on the noformat flag */
  }
}                    /* FMformat */
