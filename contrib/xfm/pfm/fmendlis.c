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
*  Modified: 10 Dec 1992 - To use AFI routines for ansi compatability
*            12 May 1993 - To reduce bullet size to textsize/2.
*             6 Jun 1993 - To output bullet characters in octal (\xxx).
*             6 Apr 1994 - To convert to pure points.
*             4 May 1994 - To use block->size value for adjustments
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
   for( liptr = lilist; liptr != NULL; liptr = liptr->next )
    {                       /* run each of the block and place item marks */
     if( liptr->ypos[0] > 0 )              /* if something set on this block */
      {
       FMsetfont( liptr->font, liptr->size );   /* set font for this block */
	FMfmt_add( );				/* add a formatting block to the list */
      }
     for( i = 0; i < MAX_LIITEMS && liptr->ypos[i] > 0; i++ ) /* run list */
      {
       sprintf( outbuf, "%d -%d moveto (\\%o) show\n", liptr->xpos,
                      liptr->ypos[i], (unsigned int) liptr->ch );
       AFIwrite( ofile, outbuf );                   /* write it to the file */
       liptr->ypos[i] = -1;                         /* turn off this mark */
      }                      /* end for each item on this block */
     liptr->yindex = 0;      /* start back at the beginning */
    }                        /* end for each block in the list */

   if( option == TRUE )       /* not just end of pg - delete the block too */
    {
     lmar = lilist->xpos;          /* reset the margin */
     linelen += lilist->size + 3;  /* and line length */
     liptr = lilist;               /* prep to delete the head list item */
     lilist = liptr->next;
     free( liptr->font );          /* free the font that was allocated */
     free( liptr );                /* delete the block */
    }
  }   /* end if liliptr != NULL */

}   /* FMendlist */
