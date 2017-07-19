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
*  Mnemonic: FMtoksize
*  Abstract: This routine will calculate an estimated token size in points
*            based on the current text size set.
*  Parms:    tok - Pointer to the start of the token
*            len - lengtht of the token
*  Returns:  Integer number of points required to display the token.
*  Date:     6 April 1994
*  Author:   E. Scott Daniels
*
*****************************************************************************
*/
/* best guess if we cannot figure out a table in font.h based on name */
int unknown[ ] =  /* point sizes for 100 point characters */
 {
  32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
  32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
  32, 30, 38, 62, 62, 90, 80, 22, 30, 30, 44, 60, 32, 40, 32, 60,
  62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 32, 32, 60, 60, 60, 54,
  82, 68, 74, 74, 80, 72, 64, 80, 80, 34, 60, 72, 60, 92, 74, 80,
  62, 82, 72, 66, 62, 78, 70, 96, 72, 64, 64, 30, 60, 30, 60, 50,
  22, 58, 62, 52, 62, 52, 32, 54, 66, 30, 30, 62, 30, 94, 66, 56,
  62, 58, 44, 52, 38, 68, 52, 78, 56, 54, 48, 28, 60, 28, 60, 32,
  32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
  32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
  32, 30, 62, 62, 14, 62, 62, 52, 62, 22, 40, 36, 24, 24, 62, 62,
  32, 50, 54, 54, 32, 32, 60, 46, 22, 40, 40, 36,100,128, 32, 54,
  32, 34, 34, 42, 44, 44, 46, 26, 42, 32, 32, 32, 32, 38, 32, 42,
 100, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
  32,126, 32, 42, 32, 32, 32, 32, 60, 80,124, 42, 32, 32, 32, 32,
  32, 86, 32, 32, 32, 30, 32, 32, 32, 56, 90, 66, 32, 32, 32, 32,
 };          /* end character width table */

#include	"fonts.h"		/* widts for various fonts */

FMtoksize( tok, len )
 char *tok;
 int len;
{
 int i;            /* loop index */
 int size = 32;    /* calculated size - start assuming a blank seperates */
 int *charwidth = unknown;

 if( strcmp( curfont, "Courier" ) == 0 )
	return ((80 * strlen( tok ))* textsize)/100;		/* all chars are the same width */

 if( strcmp( curfont, "Times-Roman" ) == 0 )
	charwidth = TimesRoman;
 else
 if( strcmp( curfont, "Times" ) == 0 )
	charwidth = TimesRoman;
 else
 if( strcmp( curfont, "Times-Bold" ) == 0 )
	charwidth = TimesBold;
 else
 if( strcmp( curfont, "Times-Italic" ) == 0 )
	charwidth = TimesItalic;
 else
 if( strcmp( curfont, "Helvetica" ) == 0 )
	charwidth = Helvetica;
 else
 if( strcmp( curfont, "Helvetica-Bold" ) == 0 )
	charwidth = HelveticaBold;
 else
 if( strcmp( curfont, "Helvetica-Italic" ) == 0 )
	charwidth = HelveticaItalic;
 else
 if( strcmp( curfont, "Helvetica-Oblique" ) == 0 )
	charwidth = HelveticaItalic;
	
 if( len <= 0 )
  size = 0;            /* empty token has no size */
 else
  {
   for( i = 0; i < len; i++ )
    size += charwidth[tok[i]];       /* add value from table */

   size = (size * textsize) / 100;   /* adjust size for text size */
  }

 return( size );       /* send the value back */
}                   /* FMtoksize */
