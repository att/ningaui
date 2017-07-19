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
*  Mnemonic: FMread
*  Abstract: This routine reads a record from the proper input file in
*            the input list.
*  Parms:    buf - Pointer to the buffer to read the record into
*  Date:     15 November 1988
*  Author:   E. Scott Daniels
*
*  Modified: 5 May 1989 - To place an EOS in buf[0] before read (solves the
*                         hanging problem when in noformat mode and EOF reached)
*           16 Mar 1990 - To use UTread inplace of FIread.. Fi package has a
*                         bug.
*           10 Dev 1990 - To use AFI fileio routines (using ansi std f rtns)
*            3 Apr 1997 - To convert for AFI tokenizer!
***************************************************************************
*/
int FMread( buf )
 char *buf;
{
 int status = ERROR;    /* read status */

 if( (status = AFIread( fptr->file, buf )) < AFI_OK )
  *buf = EOS;

 return( status );
}
