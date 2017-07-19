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
*  Mnemonic: FMgetpts
*  Abstract: This routine will convert a valued entered by the user into
*            points. The token passed in is assumed to be a number that may
*            be postfixed with a 'p' (points) or an i (inches). If the value
*            is in inches (postfix i) then the value may be a real number.
*  Parms:    tok - Pointer to token to look at
*            len - Length of the token to look at
*  Returns:  Number of points that the token converted to.
*  Date:     6 April 1994
*  Author:   E. Scott Daniels
*
*****************************************************************************
*/
int FMgetpts( tok, len )
 char *tok;
 int len;
{
 int pts = 0;         /* points to return */
 double fval;          /* value for converting to float */

 switch( tok[len-1] )   /* look at last character of the token */
  {
   case 'i':                    /* units are inches */
   case 'I':
    fval = atof( tok );         /* convert to float */
    fval = fval * 72;           /* convert to floating points */
    pts = fval;                 /* convert to integer points */
    break;

   case 'p':                    /* user entered points */
   case 'P':
    pts = atoi( tok );          /* direct conversion to points */
    break;

   default:                     /* nothing specified - assume characters */
    pts = atoi( tok );          /* convet to integer */
    pts *= PTWIDTH;             /* convert to points */
    break;
  }  /* end switch */

 return( pts );           /* and send back calculated value */
}                     /* FMgetpts */
