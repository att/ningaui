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

/* DEPRICATED - with the implementation of the symbol table to map and 
   fetch variables, this routine is no longer necessary!
*/

errors should happend if compiled accidently
/*
****************************************************************************
*
*   Mnemonic: FMgetvar
*   Abstract: This routine is responsible for finding the name passed in
*             in the list of defined variables. If the name is not found
*             in the list an error message is generated.
*   Parms:    name - Pointer to the string to find.
*             len  - Length of the name string (not guarenteed to be EOSed)
*   Returns:  Returns a pointer to the buffer string that the variable 
*             expands to.
*   Date:     7 July 1989
*   Author:   E. Scott Daniels
*
*   Modified: 9-01-89 - To support user defined variable delimiter.
*            12-15-92 - To correct name for compatability with case sensitive
*                       linkers.
*             7-28-94 - To allow punctuation appended to var name
*             8-24-94 - To return pointer to found blk for redefining vars.
*            10-13-94 - To use the utformat routine to expand the var thus
*                       allowing for parameterized variables:
*                         .dv h1 .rh $1 ^: .h1 $1
*                         &h1(Header string also running header)
*                         generates: .rh Header string also running header
*                                    .h1 Header string also running header
*             2-06-95 - To find last ) in a paramaterized variable
*             4-03-97 - To use the new AFI tokenizer!
*****************************************************************************
*/
char *FMgetvar( name, len )       /* return pointer to expansion string */
 char *name;
 int len;
{
 char tbuf[80];              /* temp buffer for compare */
 struct var_blk *vptr;       /* pointer into the list of variable blocks */

 if( *name == vardelim )
  {
   name++;                  /* skip over the variable symbol */
   len--;
  }

 len = len > MAX_VARNAME ? MAX_VARNAME : len;  /* dont go max name len*/

 strncpy( tbuf, name, len );   /* get the name to look for */
 tbuf[len] = EOS;              /* make it a real string */

 for( vptr = varlist; vptr != NULL && strcmp( vptr->name, tbuf ) != 0;
      vptr = vptr->next );

 if( vptr != NULL )            /* found one */
  return( vptr->string );
 else
  return( NULL );

}         /* FMgetvar */
