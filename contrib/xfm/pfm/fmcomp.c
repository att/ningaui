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
*  Mnemonic:  FMcomp
*  Abstract:  This file compiles the entire PostScript Text Formatting
*             (PSFM) utility. Source modules that were modified to generate
*             PSFM are included from the current directory. If an original
*             TFM source module is being used unchanged then it is included
*             from the /tfm directory
*  Date:      26 April 1992
*  Author:    E. Scott Daniels
*
***************************************************************************
*/

#include "a:\mc\include\ctype.h"
#include "globals.h"
#include "fileio.h"

#include "FMCONST.H"
#include "FMSTRUCT.H"

#include "FMREAD.C"
#include "FMCLOSE.C"
#include "FMOPEN.C"
#include "FMIMBED.c"
#include "FMSPACE.C"
#include "FMADDTOK.C"
#include "FMMAIN.C"
#include "FMFLUSH.C"
#include "FMGETTOK.C"
#include "FMGETPAR.C"
#include "FMMSG.C"
#include "fminit.c"
#include "fmll.c"
#include "fmcmd.c"
#include "fmpflush.c"        /* page flush - 12-1-88 */
#include "fmcd.c"            /* colum define command processor */
#include "fmheader.c"        /* paragraph header 12-1-88 */
#include "fmhn.c"            /* turn on/off header numbers 12-2-88 */
#include "fmtopmar.c"        /* adjust top margin  12-3-88 */
#include "fmcpage.c"         /* conditional page eject 12-3-88 */
#include "fmccol.c"          /* condtional col eject  12-3-88 */
#include "fmindent.c"        /* indent next line 12-3-88 */
#include "fmrhead.c"         /* running header  12-3-88 */
#include "fmrfoot.c"         /* running footer  12-3-88 */
#include "fmpgnum.c"         /* page number toggle 12-3-88 */
#include "fmnofmt.c"         /* no format processing 12-8-88 */
#include "fmformat.c"        /* handle format command 12-8-88 */
#include "fmfigure.c"        /* generate figure ending and number 12-9-88 */
#include "fmsetjus.c"        /* set the just flag  12-10-88 */
#include "fmjustif.c"        /* justify a line of text  12-10-88 */
#include "fmtc.c"            /* turn on or off the toc  12-21-88 */
#include "fmtoc.c"           /* process a toc entry  12-21-88 */
#include "fmbd.c"            /* begin a definition list 1-1-89 */
#include "fmditem.c"         /* process a definition item  1-1-89 */
#include "fmbeglst.c"        /* begin a regular list  4-30-89 */
#include "fmbox.c"           /* process a box command 5-5-89 */
#include "fmbxst.c"          /* start a box 5-5-89 */
#include "fmbxend.c"         /* end a box 5-5-89 */
#include "fmvartok.c"        /* get a token from variable expansion 7-7-89 */
#include "fmdv.c"            /* handle defination of a variable 7-7-89 */
#include "fmgetvar.c"        /* set a variable for expansion 7-7-89 */
#include "fmrunout.c"        /* print running material 5-4-91 */
