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
*   Mnemonic:  FMcindent
*   Abstract:  This routine causes a break and beginning with the next line
*              sets the left margin according to the parameter.
*   Parms:     mar - pointer to margin to adjust (heaer or left mar )
*   Returns:   Nothing.
*   Date:      3 December 1988
*   Author:    E. Scott Daniels
*
*   Modified:   5 May 1992 - To support postscript conversion.
*              24 Mar 1994 - To allow amount to be specified in pts or inches
*               6 Apr 1994 - To use FMgetpts routine to convert value to pts
*                            so n can be postfixed with p (points) or i (in).
*   .in n | +n | -n  (indention not changed if n is omitted)
***************************************************************************
*/
void FMindent( mar )
 int *mar;
{
 char *buf;          /* pointer at the token */
 int len;
 int flags = 0;

 len = FMgetparm( &buf );    /* is there a length? */

 if( len == 0 )              /* no parameter entered by user */
  return;

 len = FMgetpts( buf, len );    /* convert value entered to points */

 if( buf[0] == '+' || buf[0] == '-' )  /* add/sub parm to/from cur setting */
  /*len = *mar + len; */            /* add pos/neg value to current setting */
  *mar += len;                     /* "add" current setting to value entered */
 else
  {                                  /* reset mar to specific vlaue */
   if( len < 0 || len > MAX_X )                 /* if out of range */
    {
printf( "INDENT: out of range len = %d buf = (%s)", len, buf );
     FMmsg( E_TAKEOUT, buf );
     return;
    }
   else
    *mar = cur_col->lmar + len;     /* base on the current column */
  }                    /* end else - +/- sign not there */


/* *mar = len;    */         /* reset the line length and go */
}                        /* FMindent */
