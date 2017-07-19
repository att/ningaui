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
*    Mnemonic: FMpflush
*    Abstract: This routine causes the running header and footer to be placed
*              into the file and then issues the newp command which will issue
*              a showpage and then a translate.
*    Parms:    None.
*    Returns:  Nothing
*    Date:     1 December 1988
*    Author:   E. Scott Daniels
*
*    Modified: 29 Jun 1994 - To convert to rfm
*               8 Dec 1994 - To put a para mark before page if not there
*                            and if not in a list item list.
*****************************************************************************
*/
void FMpflush( )
{
 int diff;                   /* difference between default lmar and cur lmar*/
 int diffh;                  /* difference between default lmar and header */
 int i;                      /* index into page number buffer */
 char tbuf[HEADFOOT_SIZE+1]; /* buf to gen page and head/foots in  */
 int len;                    /* number of characters in the string */
 int even = FALSE;           /* even/odd page number flag */
 int skip;                   /* # columns to skip before writing string */
 int shift = FALSE;          /* local flag so if stmts are executed only once */


#ifdef NO
 if( (rflags & RF_PAR) == 0 && lilist == NULL ) /* if text since last mark */
  AFIwrite( ofile, "\\par" );
#endif

 page++;                             /* increase the page number */
 AFIwrite( ofile, "\\page" );        /* cause printer to eject */
/* FMpara( 0, FALSE ); */          /* start up next "paragraph" */


                                 /* determine if we need to shift the page */
 diff = lmar - cur_col->lmar;   /* calculate diff in col default mar& cur mar */
 diffh = hlmar - cur_col->lmar;  /* calc diff in header left margin */
 cur_col = firstcol;             /* start point at the first column block */
 lmar = cur_col->lmar + diff;    /* set up lmar for first column */
 hlmar = cur_col->lmar +diffh;   /* set up header left margin for first col */

 if( rtopy != 0 )
  {
   topy = rtopy;               /* reset as temp y only good to end of page */
   rtopy = 0;                  /* indicate nothing set at this point */
  }
 cury = topy;                  /* reset current y to top y position */
}                              /* FMpflush */
