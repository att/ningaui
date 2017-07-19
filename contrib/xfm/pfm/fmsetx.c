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
* *** WARNING: With the additon of the fmt blocks, this is likely broken!!!
* 
*  Mnemonic: FMsetx
*  Abstract: This routine is responsible for adjusting the current x position
*            on the output page. The value entered may be specified in pts or
*            inches. A + or - prefix to the value indicates a relitive move,
*            and a value without + or - indicates an absolute position in
*            relation to the current column's left margin value.
*  Params:   None.
*  Returns:  Nothing.
*  Date:     3 December 1992
*  Author:   E. Scott Daniels
*
*  Modified: 10 Dec 1992 - To use AFI routines for ansi compatability
*            25 Mar 1992 - To accept a + for relative moves
*            22 Apr 1993 - To set relmove flag initially to false
*             7 Apr 1994 - To use the getpts routine allowing user to enter
*                          x value in inches or points.
*            11 Apr 1994 - To take value relative to curcol's left margin
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
   if( buf[0] == '+' )    /* relative move? */
    relmove = TRUE;       /* then set local flag */

   i = FMgetpts( buf, len );

   if( i < cur_col->width )  /* ensure its in the column */
    {
     if( optr == 0 )   /* if nothing currently in the buffer */
      {
       optr = 1;
       obuf[0] = ' ';        /* force flush to setup the current y */
       obuf[1] = EOS;
       osize = 0;            /* reset output size */
      }

     FMflush( );             /* put out what is already there */

     if( relmove == TRUE ) /* if relative move */
      {
       osize += i;             /* indicate number of points skipped over */
       sprintf( wbuf, "%d 0 rmoveto\n", i );        /* ps to relative move */
      }
     else
      {
       osize = i;              /* indicate position in obuf in terms of pts */
       sprintf( wbuf, "%d -%d moveto\n", cur_col->lmar + i, cury );
      }

     AFIwrite( ofile, wbuf );                        /* write to file */
    }                                                /* end if in range */
   else            /* generate an error message */
    FMmsg( E_PARMOOR, ".sx" );
  }                                                  /* end if parm entered */
}                                 /* FMsetx */
