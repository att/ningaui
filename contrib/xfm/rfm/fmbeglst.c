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
*  Mnemonic: FMbeglst
*  Abstract: This routine is responsible for adding a list item block to the
*            list item list.
*  Parms:    Nothing. It reads the input buffer to get the character that is
*            to be used, and the optional font name.
*  Returns:  Nothing.
*  Date:     19 October 1992
*  Author:   E. Scott Daniels
*
*      .bl [character | \nnn] s=size f=fontname
*
*  Modified:  1 Jul 1994 - To convert to rfm
*                          made parameters non positional.
*            25 Nov 1996 - Correct problems picking up f= parameter
******************************************************************************
*/
void FMbeglst( )
{
 struct li_blk *new;     /* new allocated block */
 char *buf;              /* pointer at next tokin in buffer */
 int len;                /* length of parameter input */
 int i;                  /* loop index */

 FMflush( );              /* kick out whate ever is there */

 new = (struct li_blk *) malloc( sizeof( struct li_blk ) );  /* get space */
 if( new == NULL )
  {
   printf( "BEGLST: new was null on malloc\n" );
  }
 new->next = lilist;      /* point at whats there (if anything) */
 new->size = textsize/2;  /* default to 1/2 the current text size */
 new->ch = DEF_LICHR;     /* initial li character default */
 new->font = NULL;        /* no font yet */
 lilist = new;            /* put this at the head of the list */

 while( (len = FMgetparm( &buf )) != 0 )  /* get parameters */
  {
   if( toupper( buf[0] ) == 'S' && buf[1] == '=' )   /* size parameter */
    new->size = FMgetpts( &buf[2], len-2 );    /* convet to points */
   else
    if( buf[0] == '\\' )               /* if escape character not entered */
     {
      new->ch = (char) atoi( &buf[1] );  /* xlate decimal value input */
      if( new->ch == 0x04 )
       new->ch = '*';                 /* dont allow control d to be entered */
     }
    else
     if( toupper( buf[0] ) == 'F'  && buf[1] == '=' )  /* font entered */
      {
       if( len > 79 )
        len = 79;                                   /* dont bring us down */
       new->font = (char *) malloc( (unsigned) 80 );
       strncpy( new->font, &buf[2], len-2 );        /* copy in the string */
       new->font[len-2] = EOS;                      /* mark its end */
      }
     else
      new->ch = buf[0];        /* just use character that was entered */
  }                            /* end parameter grabbing while loop */

 if( new->font == NULL )   /* user did not enter font on .bl - use default */
  {
   new->font = (char *) malloc( (unsigned) 80 );
   strcpy( new->font, DEF_LIFONT );
  }

 for( i = 0; i < MAX_LIITEMS; i++ )
  new->ypos[i] = -1;         /* reset all item values */

 new->xpos = lmar;           /* set up on the current left margin */

 new->yindex = 0;            /* set up y position index */

 lmar += new->size + 5;       /* scoot over where text will be placed */
 linelen -= new->size + 5;    /* reduce number of points per line */

}           /* FMbeglst */
