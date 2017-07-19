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
******************************************************************************
*
* TFM
*  Mnemonic: FMendlist
*  Abstract: This routine is responsible for putting out the list item
*            marks that are defined in the current list item blocks.
*            As each mark is putout its place in the ypos array is reset.
*            If the option true is passed in the current (top) list item
*            block is deleted. This allows this routine to be called from
*            FMcflush to paint all of the item marks on the page, without
*            really terminating the list.
*            Because this routine is called from FMceject to putout item
*            marks in the current column, this routine must NOT call flush
*            as flush calls ceject and we stackoverflow.
*  Parms:    option - If true then top list item block is removed.
*  Returns:  Nothing
*  Date:     19 October 1992
*  Author:   E. Scott Daniels
*
*  Modified:  1 Jul 1994 - To convet to rfm
*             6 Dec 1996 - To convert to hfm
*            21 Mar 2001 - Tracing our roots into the soil; TFM
******************************************************************************
*/
void FMendlist( option )
 int option;
{
 struct li_blk *liptr;       /* pointer at list item block to delete */
 char outbuf[80];            /* buffer to build output string in */
 int len;                    /* length of output buffer */
 int i;                      /* loop index */

 if( lilist != NULL )    /* if list has been started */
  {
   if( option == TRUE )       /* not just end of pg - delete the block too */
    {
     liptr = lilist;                   /* prep to delete the head list item */
     lilist = liptr->next;
     if( liptr )
      free( liptr );                    /* delete the block */
#ifdef KEEP
     strcat( obuf, "</ul>" );   /* put in html end list tag */
     optr += 5;
#endif
    }
  }   /* end if liliptr != NULL */

}   /* FMendlist */
