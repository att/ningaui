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
*****************************************************************************
*
*  TFM
*  Mnemonic: FMjustify
*  Abstract: This routine is responsible for sending out the current buffer
*            with the proper justification command and related parameters.
*  Parms:    None.
*  Returns:  Nothing.
*  Date:     12 November 1992
*  Author:   E. Scott Daniels
*
*  Modified: 04 Dec 1992 - To call flush if spaces are 0
*            10 Dec 1992 - To use AFI routines for ansi compatability
*             6 Apr 1994 - To take advantage of linelen being points now
*            23 Mar 2001 - Drastically modified to go back to TFM
*****************************************************************************
*/
void FMjustify( )
{
 static int direction = 0;   /* direction of travel on spaces */

 int spaces = 0;        /* number of blank spaces */
 int i;             /* loop index */
 int j;
 int len;           /* length of the string for write */
 int need;          /* number of spaces that must be inserted */
 int add;           /* number to add to each existing space */
 int start = 0;     /* which token to start with */
 char *tok;         /* pointer at token */
 char jbuf[2048];   /* buffer to build in */


 if( ! (flags2 & F2_OK2JUST ) )
  return;

 for( i = 0; obuf[i] != EOS; i++ )    /* find end of the string */
  if( obuf[i] == ' ' )
   spaces++;                            /* count spaces as we go */

 for( i--; obuf[i] == BLANK; i-- );   /* find last non blank */
  spaces--;
 obuf[i+1] = EOS;                     /* ensure no blanks at end of str */

 need = (linelen/7) - strlen( obuf );   /* number of extra spaces at end */

 if( spaces == 0 )
  return;             /* none to do */

 add = need / spaces;

 if( add > 2 )             /* just cannot see the need to add more than this */
  return;                      /* just plain too many to add */

 if( add * spaces < need )   /* correct for rounding problem */
  add++;

/*
fprintf( stderr, "just: need=%d spaces=%d add=%d buf=(%s)\n", need, spaces, add, obuf );
*/

 jbuf[0] = 0;
 tok = strtok( obuf, " " );
 start = direction ? spaces - need : 0;
 direction = direction ? 0 : 1;

 while( tok )
  {
   strcat( jbuf, tok );
   strcat( jbuf, " " );
   if( --start < 0 )
    for( j = add; j > 0 && need > 0; j--, need-- )
     strcat( jbuf, " " );
    
   tok = strtok( NULL, " " );

   spaces--;                        /* makes things look nicer if we adjust */
   if( add * spaces > need && (add-1) * spaces >= need )
    add--;
  }

 strcpy( obuf, jbuf );
}           /* FMjustify */
