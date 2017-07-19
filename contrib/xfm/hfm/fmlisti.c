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
******************************************************************************
*
*  Mnemonic: FMlisti
*  Abstract: This routine will setup for a list item in the current text.
*  Parms:    None.
*  Returns:  Nothing.
*  Date:     1 July 1994
*  Author:   E. Scott Daniels
*
*  Modified: 19 Jul 1994 - To allow list items in boxes
*             8 Dec 1995 - To use font in list item block
*             6 Dec 1996 - To convert to hfm
******************************************************************************
*/
void FMlisti( )
{
 struct li_blk *lptr;         /* pointer into listitem list */
 char buf[81];                /* output buffer */
 int right;                   /* right indention value */

 lptr = lilist;           /* point at the list */

 FMflush( );

 if( lptr != NULL )
  {
   strcat( obuf, "<li>" );
   optr += 4;                       /* add tag and scoot past it */
  }

}             /* FMlisti */
