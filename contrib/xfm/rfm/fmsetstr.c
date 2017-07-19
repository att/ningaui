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
*   Mnemonic:  FMsetstr
*   Abstract:  This routine builds a string from the parameter list.
*              If the pointer passed in is NULL then the buffer is allocated.
*              If no more parameters exist on the input line then the string
*              is freed.
*              This routine is needed (as opposed to doing a string copy into
*              string) to expand variables and take literals (between ` `) by
*              using FMgetparm.
*   Parms:     string - Pointer to the pointer to the string to fill.
*              slen - Maximum string length
*   Returns:   Nothing.
*   Date:      6 June 1993
*   Author:    E. Scott Daniels
*
***************************************************************************
*/
void FMsetstr( string, slen )
 char **string;
 int slen;
{
 char *buf;          /* pointer at the token */
 char *lstring;      /* local pointer at the string */
 int i;              /* pointer into the token string */
 int j;              /* pointer into the footer buffer */
 int len;            /* length of the token */

 if( (lstring = *string) == NULL )        /* no buffer yet */
  lstring = *string = (char *) malloc( (unsigned) slen );

 len = FMgetparm( &buf );       /* get 1st parameter */
 if( len == 0 )
  {
   if( lstring )
    free( lstring );
   *string = NULL;         /* no parameter means no string */
   return;
  }

 j = 0;                   /* initiaize j to index into buffer at 0 */
 do                       /* put parameters into the buffer */
  {
   for( i = 0; i < len && j < slen-1; j++, i++ )
    lstring[j] = buf[i];
   len = FMgetparm( &buf );   /* get next parameter */
   if( len > 0 )              /* if another token on parameter line */
    lstring[j++] = ' ';       /* then seperate with a blank */
  }
 while( (len > 0)  && (j < slen-1) );  /* until max read or end of line */

 lstring[j] = EOS;      /* terminate the string */
}               /* FMsetstr */
