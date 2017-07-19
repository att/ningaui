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
*  Mnemonic: FMrunset
*  Abstract: This routine is responsible for outputting the necessary commands
*            to get the header, footer and page number going.
*            This routine should be called any time a header footer or page
*            number option is changed. (Unfortunately the page numbering is
*            left to be done by the rtf device and thus the actual number
*            cannot be controlled by rfm.)
*  Parms:    none.
*  Returns:  Nothing.
*  Date:     11 July 1994
*  Author:   E. Scott Daniels
*
*  Modified: 12 Jul 1994 - To allow page number and footer.
******************************************************************************
*/
void FMrunset( )
{
 int right;               /* value for right indent of header */
 char obuf[81];           /* work buffer for output */
 char obuf2[81];          /* second work buffer for output */
 char *fmtstr;            /* format string for sprintf */

 fmtstr = "{\\%s \\pard\\plain %s\\s15\\brdrt\\brdrs\\brdrw20\\brsp20";

 right = 20;
/* right = cur_col->width - ((lmar - cur_col->lmar) + linelen ); */
/* right -= 10; */    /* extend past text spot */

 sprintf( obuf, "\\li0\\fi0\\ri%d\\f4\\fs20", right * 20 );

 if( rfoot != NULL || (flags & PAGE_NUM) )   /* something for footer area? */
  {
   /*
   AFIwrite( ofile,
       "{\\footerr\\pard\\plain \\qr\\s15\\brdrt\\brdrs\\brdrw20\\brsp20" );
   */
   sprintf( obuf2, fmtstr, "footerr", "\\qr" );  /* set up with right just */
   AFIwrite( ofile, obuf2 );                     /* and send it out */

  /* AFIwrite( ofile, "\\margl0\\margr0\\li0\\fi0\\ri0\\f4\\fs20" ); */
   AFIwrite( ofile, obuf );

   if( rfoot != NULL )             /* if there is a footer string write it */
    {
     AFIwrite( ofile, rfoot );
     AFIwrite( ofile, "\\par" );   /* and terminate with paragraph mark */
    }

   if( flags & PAGE_NUM )    /* if numbering pages - insert the page field */
     AFIwrite( ofile, "Page {\\field{\\*\\fldinst {\\cs17 PAGE}}} \\par" );
   AFIwrite( ofile, "\\par }" );        /* terminate the footer thing */

   if( flags2 & F2_2SIDE )   /* if in two sided mode generate second footer */
    {
     sprintf( obuf2, fmtstr, "footerl", " " );  /* set up with right just */
     AFIwrite( ofile, obuf2 );                     /* and send it out */

     AFIwrite( ofile, obuf );

     if( rfoot != NULL )             /* if there is a footer string write it */
      {
       AFIwrite( ofile, rfoot );
       AFIwrite( ofile, "\\par" );   /* and terminate with paragraph mark */
      }

     if( flags & PAGE_NUM )    /* if numbering pages - insert the page field */
      AFIwrite( ofile, " Page {\\field{\\*\\fldinst {\\cs17 PAGE}}} \\par" );
     AFIwrite( ofile, "\\par }" );      /* terminate the footer thing */
    }                                   /* end if two sided mode */
  }                                     /* end if footer or page number */
 else
  {
   AFIwrite( ofile, "{\\footerr\\pard\\plain\\fs5}" );  /* turn off footers*/
   AFIwrite( ofile, "{\\footerl\\pard\\plain\\fs5}" );  /* if missing */
  }

 if( rhead != NULL )         /* is there a header to write? */
  {
   if( flags2 & F2_2SIDE )   /* if we should flip headers for 2 sided */
    {
     AFIwrite( ofile,
         "{\\headerl\\pard\\plain \\s15\\brdrb\\brdrs\\brdrw20\\brsp20" );
     AFIwrite( ofile, obuf );
     AFIwrite( ofile, rhead );
     AFIwrite( ofile, "\\par\\pard\\plain\\par}" );
    }

   AFIwrite( ofile,
       "{\\headerr\\pard\\plain \\qr\\s15\\brdrb\\brdrs\\brdrw20\\brsp20" );
  /* AFIwrite( ofile, "\\li0\\fi0\\ri-200\\f4\\fs20" ); */
   AFIwrite( ofile, obuf );
   AFIwrite( ofile, rhead );
   AFIwrite( ofile, "\\par\\pard\\plain\\par}" );
  }
 else                                /* no header just turn off */
  {
   AFIwrite( ofile, "{\\headerr\\pard\\plain\\fs5}" );  /* turn off */
   AFIwrite( ofile, "{\\headerl\\pard\\plain\\fs5}" );  /* turn off */
  }
}                /* FMrunset */
