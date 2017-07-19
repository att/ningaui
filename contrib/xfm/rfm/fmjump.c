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
*  Mnemonic: FMjump
*  Abstract: This routine will jump to the next token in the input file 
*            that matches the first token passed in following the .jm
*            The next token read will be the first token following 
*            the matched label.
*            command.
*  Parms:    Nothing.
*  Returns:  Nothing
*  Date:     14 April 1997
*  Author:   E. Scott Daniels
*
*****************************************************************************
*/
void FMjump( )
{
 char *tok;            /* pointer to the next token to check */
 char label[256];      /* buffer to put lable we seek into */
 int len;              /* length of token */
 int llen;             /* length of label */

 if( (llen = FMgetparm( &tok )) > 0 )
  {
   memcpy( label, tok, llen );


   label[llen] = EOS;

   while( (len = FMgettok( &tok )) >= 0 )                  /* get next token */
    if( len == llen && strncmp( tok, label, len ) == 0 )   /* match? */
     return;                                                /* exit loop */
  }
 else
  FMmsg( E_MISSINGPARM, ".ju" );
}               /* FMjump */

