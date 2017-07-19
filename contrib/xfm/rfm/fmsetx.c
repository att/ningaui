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
*  Mnemonic: FMsetx
*  Abstract: This routine is responsible for adjusting the current x position
*            on the output page. The value entered may be specified in pts or
*            inches. A + or - prefix to the value indicates a relitive move,
*            and a value without + or - indicates an absolute position in
*            relation to the current column's left margin value.
*            NOTE: In RFM the +/- value cannot be supportd. The value will
*                  be treated as an absolute value.
*  Params:   None.
*  Returns:  Nothing.
*  Date:     3 December 1992
*  Author:   E. Scott Daniels
*
*  Modified: 13 Jul 1994 - To convert to rfm.
******************************************************************************
*/
void FMsetx( )
{
 char *buf;            /* pointer at the parameter user has entered */
 int len;              /* length of parameter entered */
 int i;                /* new x position */
 int relmove = FALSE;  /* new x is relative to current x position */
 char wbuf[50];        /* buffer to build string in to write to file */

 if( (len = FMgetparm( &buf )) > 0 )   /* if user entered a parameter */
  {
   if( buf[0] == '+' )       /* relative move right */
    relmove = TRUE;          /* set the local flag for later */

   i = FMgetpts( buf, len );  /* get user's value in points */

   if( i < cur_col->width )  /* ensure its in the column */
    {
     if( relmove )           /* need to figure exact i to move to */
      {
       i += osize + lmar;    /* calculate the real offset for tab stop */
      }

     FMflush( );             /* put out what is already there */

     if( relmove )           /* need to figure exact i to move to */
      {
       osize = i - lmar;     /* calculate the real space on the line */
      }

     sprintf( wbuf, "\\tx%d \\tab", i * 20 );

     AFIwrite( ofile, wbuf );                        /* write to file */
    }                                                /* end if in range */
   else            /* generate an error message */
    FMmsg( E_PARMOOR, ".sx" );
  }                                                  /* end if parm entered */
}                                 /* FMsetx */
