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
**************************************************************************
*
*  Mnemonic:  FMpscomp
*  Abstract:  This file compiles the entire PostScript Text Formatting
*             (PSFM) utility. Source modules that were modified to generate
*             PSFM are included from the current directory. If an original
*             TFM source module is being used unchanged then it is included
*             from the /tfm directory
*  Date:      26 April 1992
*  Author:    E. Scott Daniels
*
*  Modified:   3 May 1992 - To support post script conversion
***************************************************************************
*/

/*
#include "globals.h"
#include "fileio.h"
*/

                       /* get include info supplied by compiler */
#include <stdio.h>     /* pickup null etc */
#include <fcntl.h>     /* pickup file control stuff */
#include <stat.h>      /* pickup defines for open etc */
#include <ctype.h>     /* get isxxx type headers */

#include "FMCONST.H"
#include "FMSTRUCT.H"
#include "fmdummy.c"    /* dummy routines we dont need, but referenced in ut*/

#include "fmsetx.c"
#include "fmasis.c"
#include "fmbeglst.c"   /* begin a regular list  4-30-89 */
#include "fmendlis.c"   /* end/show a list */
#include "fmbox.c"      /* process a box command */
#include "fmbxst.c"     /* start a box  */
#include "fmbxend.c"    /* end a box  */
#include "fmsetfon.c"   /* set font in output file 5-11-92 */
#include "fmceject.c"   /* eject the current column 5-6-92 */
#include "fmaddtok.c"   /* add a tokin to the output buffer */

#include "fmfigure.c"        /* generate figure ending  12-9-88 */
#include "fmbd.c"            /* begin a definition list 1-1-89 */
#include "fmditem.c"         /* process a definition item  1-1-89 */
#include "fmccol.c"          /* condtional col eject  12-3-88 */
#include "fmindent.c"        /* indent a margin */
#include "fmmain.c"

