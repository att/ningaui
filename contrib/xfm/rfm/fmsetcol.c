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
*  Mnemonic: FMsetcol
*  Abstract: This routine will write the necessary rich text format info for
*            multiple column definitions
*  Parms:    gutter - space between columns (points)
*  Returns:  Nothing
*  Date:     13 July 1994
*  Author:   E. Scott Daniels
*
******************************************************************************
*/
void FMsetcol( gutter )
 int gutter;
{
 struct col_blk *cptr;         /* pointer into column list */
 int i;
 char wbuf[50];                /* work buffer for output */

 if( (rflags & RF_SBREAK) == 0 )                   /* if no section break */
  {
   AFIwrite( ofile, "\\sect \\sectd \\sbkcol\\footery2" );  /* then do one */
   rflags |= RF_SBREAK;                                     /* and set flg */
  }

 for( i = 0, cptr = firstcol; cptr != NULL; cptr = cptr->next, i++); /* count*/

 sprintf( wbuf, "\\cols%d", i );   /* get number of cols */
 AFIwrite( ofile, wbuf );          /* and place in file */

 if( firstcol->next != NULL )  /* if more than one col */
  {
   for( i = 1, cptr = firstcol; cptr != NULL; cptr = cptr->next, i++ )
    {
     sprintf( wbuf, "\\colno%d\\colw%d", i, firstcol->width * 20 );
     AFIwrite( ofile, wbuf );
     if( cptr->next != NULL )    /* if more then insert gutter */
      {
       sprintf( wbuf, "\\colsr%d", gutter * 20 ); /* confert to rfm scale */
       AFIwrite( ofile, wbuf );                   /* and place into the file */
      }
    }        /* end for */
  }          /* end if multi cols */
}            /* FMsetcol */
