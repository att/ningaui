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
*   Mnemonic: FMasis
*   Abstract: This routine is called when the asis flag is set. It
*             assumes that the input in the file is postscript commands
*             and places the text from the input file directly into the
*             output file until the fo coommand is encountered.
*   Parms:    None.
*   Returns:  Nothing.
*   Date:     27 November 1992
*   Author:   E. Scott Daniels
*
*   Modified: 10 Dec 1992 - To use AFI routines for ansi support
*              9 Mar 1993 - To allow .im command to be processed too
*             20 Jul 1994 - To convert to rfm
*              1 Aug 1994 - To write directly from inbuf
*              3 Apr 1997 - To use AFI's new tokenizing support!
*******************************************************************************
*/
void FMasis( )
{
 int len;               /* length of string to put in outfile */

 while( FMread( inbuf ) >= 0 )
  {
   if( inbuf[0] == CMDSYM )
    {
     if( inbuf[1] == 'f' && inbuf[2] == 'o' )  /* start formatting command */
      {
       iptr = optr = 0;                     /* return pointing at beginning */
       AFIpushtoken( fptr->file, inbuf );          /* push to process */
       return;                                     /* get out now */
      }
     else
      if( inbuf[1] == 'i' && inbuf[2] == 'm' )  /* imbed file? */
       {
        cury += textspace + textsize;              /* assume one par mark */
        AFIpushtoken( fptr->file, inbuf+4 );       /* put file name on stack */
        FMimbed( );                                /* then go process it */
        *inbuf = EOS;                              /* reset buf to write nada*/
       }
    }                                       /* end if cmd symbol encountered */

   if( *inbuf != EOS )
    AFIwrite( ofile, inbuf );                /* write it as it came in */
  }                                         /* end while */

 iptr = optr = 0;              /* return pointing at beginning */
}              /* FMasis */
