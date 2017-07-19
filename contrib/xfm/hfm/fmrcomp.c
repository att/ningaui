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


#include "fmcmds.h"
#include "fmstruct.h"              /* structure definitions */
#include "fmproto.h"

/*
**************************************************************************
*
*  Mnemonic:  FMrcomp
*  Abstract:  This file compiles the FM routines that were changed for the
*             Rich Text formatter (RFM). The object can then be linked (first)
*             with the other psfm and tfm routines.
*  Date:      29 June 1994
*  Author:    E. Scott Daniels
*
***************************************************************************
*/

                       /* get include info supplied by compiler */
#include <stdio.h>     /* pickup null etc */
#include <fcntl.h>     /* pickup file control stuff */
//#include <sys\stat.h>      /* pickup defines for open etc */
#include <ctype.h>     /* get isxxx type headers */

#include "..\include\FMCONST.H"
#define RF_PAR     0x01            /* rfm only flags */
#define RF_RUNNCHG 0x02            /* running item has changed */
#define RF_SBREAK  0x04            /* section break has occurred sinc flush */

/*#include "fmstruct.h" */
#include "..\include\FMSTR2.H"
#include "fmcmds.h"

int rflags = RF_SBREAK;            /* global rfm only flags */
int col_gutter = 0;                /* current column gutter */


                        /* these are required before all others */
#include "fmgetvar.c"   /* expand a variable into the varbuf string */

#include "fmdefhea.c"   /* define header information */
#include "fmopen.c"     /* open the input file */
#include "fmasis.c"     /* bring in rich text stuff */
#include "fmif.c"       /* if processing */
#include "fmelse.c"     /* else processing */
#include "fmgettok.c"   /* get token from input stream */
#include "fmvartok.c"   /* return next token from variable expansion */
#include "fmdv.c"       /* define variable */

/*#include "../tfm/fmindent.c"  */

#include "fmright.c"    /* right justify a string */

#include "fmsectio.c"   /* add a non distructive section break */
#include "fmtoksiz.c"   /* calc size in points of a token */
#include "fmsetcol.c"   /* setup column information in output file */
#include "fmceject.c"   /* eject to anew column */
#include "fmsetx.c"     /* "tab" to an x position on the current line */
#include "fmrunset.c"   /* set the running string */
#include "fmaddtok.c"   /* add a token to the output buffer */
#include "fmnofmt.c"    /* no format of text routine */
#include "fmline.c"     /* draw a line in the text */
#include "fmsetmar.c"   /* set the margins based on col width */
#include "fmlisti.c"    /* setup for a list item */
#include "fmcmd.c"      /* command routine start */
#include "fmcmd1.c"     /* must follow fmcmd */
#include "fmcmd2.c"     /* must follow cmd1 */
#include "fmendlis.c"   /* terminate listitem list */
#include "fmbeglst.c"   /* begin a listitem list */
#include "fmheader.c"   /* generate a header */
#include "fmbox.c"      /* box support routines */
#include "fmbxst.c"
#include "fmbxend.c"
#include "fmpara.c"    /* setup for new paragraph */
#include "fmpflush.c"  /* flush the page */
#include "fmflush.c"   /* flush the buffer */
#include "fminit.c"    /* initalize the system */
#include "fmspace.c"   /* add blank space tothe doc */
#include "fmsetfon.c"  /* set the curent text font and text size */
#include "fmditem.c"   /* add a definition item to the document */
#include "fmmain.c"    /* main driver */
#include "fmcd.c"      /* change column definitions */
