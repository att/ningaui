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


#include "fmcmds.h"
#include "fmstruct.h"              /* structure definitions */
#include "fmproto.h"

/*
****************************************************************************
*
*   Mnemonic:  FMspace
*   Abstract:  This routine processes the .SP command when entered.
*   Parms:     None.
*   Returns:   Nothing.
*   Date:      15 November 1988
*   Author:    E. Scott Daniels
*   Modified:  29 Jun 1994 - To support rfm conversion
*              18 Jul 1994 - To allow listitems in boxes
*               6 Dec 1996 - To convert for hfm
***************************************************************************
*/
void FMspace( )
{
 char *buf;          /* pointer at the token */
 int len;

 FMflush( );          /* flush out the current line */

 len = FMgetparm( &buf );    /* is there a length? */

 if( len <= 0 )     /* no parameter entered */
  len = 1;          /* defalut to one space */
 else
  len = atoi( buf );   /* convert parameter entered */
 if( len > 55 )
  len = 55;           /* no more than 55 please */


 for( ; len > 0; len-- )          /* skip lines */
  FMpara( 0, 0 );                 /* force a new "paragraph" for each blank */
}               /* FMspace */
