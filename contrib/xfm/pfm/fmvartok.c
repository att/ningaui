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
**************************************************************************
*
*   Mnemonic: FMvartok
*   Abstract: This routine checks to see if a variable is being expanded
*             and if one is then it returns a pointer to the next token
*             in the expansion string.
*   Parms:    buf - Pointer to a character pointer
*   Returns:  The length of the token or ERROR if no tokens remaining.
*             buf will point at the token.
*   Date:     7 July 1989
*   Author:   E. Scott Daniels
*
*   Modified: 26 Oct 1992 - To return token length of 0 when single : is
*               encountered. This allows empty commands in variables (commands
*               that may allow user to omit parameters)
*             28 Jul 1994 - To free the var buffer when end is reached.
***************************************************************************
*/
FMvartok( buf )
 char **buf;
{
 int tokstart;            /* offset in expansion string where token starts */

 if( varbuf != NULL )     /* variable in progress? */
  {
   for( ; varbuf[viptr] == BLANK; viptr++ );    /* find next nonblank */
   if( varbuf[viptr] != EOS )                 /* at least one more token */
    {
     for( tokstart = viptr; varbuf[viptr] != BLANK && varbuf[viptr] != EOS;
          viptr++ );              /* find token end */
     *buf = &varbuf[tokstart];    /* point at token from expansion buffer */
     if( viptr-tokstart == 1 && varbuf[tokstart] == ':' )   /* single :? */
      return( ERROR );      /* yes - simulate no parameters */
     else
      return( viptr-tokstart );   /* return token length */
    }                             /* end search for next token */
   free( varbuf );                /* dont need buffer anymore */
  }                               /* end var in progress */

 varbuf = NULL;           /* reset the pointer if no more */
 return( ERROR );         /* indicate nothing from expansion */
}                         /* FMvartok */
