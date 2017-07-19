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
*   Mnemonic:  FMccol
*   Abstract:  This routine conditially ejects the current col based on
*              the number of lines remaining in the column and the parameter
*              that the user enteres.
*   Parms:     skip - Number of lines that must be present or skip to next
*                     col is done. If 0 then assumed that cc command entered
*                     and skip value should be read from input buffer.
*   Returns:   Nothing.
*   Date:      3 December 1988
*   Author:    E. Scott Daniels
*   Modified:  23 Nov 1992 - To support postscript conversion
*                            To accept a parameter - if not 0 dont read parm
*                            use parameter instead.
***************************************************************************
*/
void FMccol( skip )
 int skip;
{
 char *buf;          /* pointer at the token */
 int len;

 FMflush( );         /* put out what may be there */

 if( skip == 0 )          /* calling routine did not pass parameter to use */
  if( FMgetparm( & buf ) > 0 )       /* then assume user command and get it */
   skip = atoi( buf );               /* convert to integer */
  else
   skip = 2;                         /* otherwise default to 2 lines */

 if( ((skip * (textsize + textspace)) + cury) >= boty )
  FMceject( );                        /* eject col if not enough there */
}               /* FMccol */
