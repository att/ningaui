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
*  Mnemonic: FMcenter
*  Abstract: This routine will put the rest of the line of text entered with
*            this command (.ce) in the center of the current column using
*            the cen macro defined in the postscript output file.
*            If the first two tokens are numbers then the text is centered
*            between these points.
*  Parms:    None.
*  Returns:  Nothing.
*  Date:     2 November 1992
*  Author:   E. Scott Daniels
*
*            .ce [x1 x2] <text to center> <eos>
*
*  Modified: 25 Mar 1993 - To accept points between which text is centered
*            28 Apr 1993 - To set same y flag if move to not necessary
*	     08 Feb 2002 - To ditch the x1 x2 parms as it seems to always cause 
*			issues. now accepts x1= x2= parameters if user really
*			wants to do this. Should cause less headaches
*****************************************************************************
*/
void FMcenter( )
{
 char *buf;          /* pointer at parameters */
 int i;              /* length of the token */

 FMflush( );                      /* write what ever might be there */

 flags2 |= F2_CENTER;   /* make flush center rather than show */

 if( (i = FMgetparm( &buf )) > 0  && strncmp( buf, "x1=", 3 ) == 0 )
  {
   flags2 |= F2_SAMEY;      /* indicate same y value */
   cenx1 = atoi( buf+3 );     /* convert first point to integer */
   if( FMgetparm( &buf ) > 0 && strncmp( buf, "x2=", 3 ) == 0 )
    cenx2 = atoi( buf+3 );    /* convert second digit */
   else
    {
     FMmsg( E_MISSINGPARM, inbuf );   /* missing parameter error */
     return;
    }
   if( cenx2 <= cenx1 )   /* cannot have this */
    {
     FMmsg( E_PARMOOR, inbuf );   /* send error and exit from here */
     return;
    }
   i = FMgetparm( &buf );    /* get first text parm to center */
  }
 else                        /* no columns defined - center between margins */
  {
   cenx1 = cur_col->lmar;
   cenx2 = cur_col->lmar + cur_col->width;  /* defalult center */
  }


 while( i > 0 )    /* until all parameters gone */
  {
   FMaddtok( buf, i );          /* add it to the output buffer */
   i = FMgetparm( &buf );
  }                             /* end while */

 FMflush( );                             /* send user line on the way */

 flags2 &= ~(F2_CENTER | F2_SAMEY);  /* turn off flags */
}      /* fmcenter */
