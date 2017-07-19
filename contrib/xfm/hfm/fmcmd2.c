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
***************************************************************************
*
*  Mnemonic: FMcmd2
*  Abstract: This file is a continuation of the FMcmd.c file as it was
*            getting two big to manage. Because of compiler requirements
*            regarding nested include files this file is simply a continuation
*            rather than being included in the fmcmd.c file
*
*  Modified:  1 Jul 1994 - To convert for rfm
*             2 Sep 1994 - To reset bottom y before head/foot page cmds.
*             8 Dec 1994 - To make a hard par mark on .pa command
*             6 Dec 1996 - To convert to hfm
*             7 Oct 1999 - To strip para call from ll as we dont control line length
****************************************************************************
*/
   case C_LINESIZE:               /* set line size for line command */
    if( FMgetparm( &buf ) > 0 )   /* get the parameter */
     {
      linesize = atoi( buf );     /* convert to integer */
      if( linesize > 10 )
       linesize = 2;              /* dont allow them to be crazy */
     }
    break;

   case C_LOWERCASE: 
     while( (i = FMgetparm( &buf )) > 0 )    /* until all parameters gone */
      {
       for( j = 0; j < i; j++ )
        buf[j] = tolower( buf[j] );
       FMaddtok( buf, i );          /* add it to the output buffer */
      }                             /* end while */
     break;

   case C_LISTITEM:         /* list item entered */
    if( lilist != NULL )
     FMlisti( );            /* output what we have so far */
    break;

   case C_LL:               /* reset line length */
    FMflush( );             /* terminate previous line */
    FMll( );                /* get and set line size parameters */
    break;

   case C_LINE:             /* put a line from lmar to col rmar */
    FMline( );              /* so do it! */
    break;

   case C_OUTLINE:          /* use true charpath and fill instead of stroke */
    if( FMgetparm( &buf ) > 0 )  /* get the parameter on | off */
     {
      if( toupper( buf[1] ) == 'N' )
       flags2 |= F2_TRUECHAR;        /* turn on the flag */
      else
       flags2 &= ~F2_TRUECHAR;       /* turn off the flag */
     }
    break;

   case C_NEWLINE:                    /* force new line in output */
    FMflush( );                       /* send last formatted line on its way */
    break;

   case C_NOFORMAT:                   /* turn formatting off */
    FMflush( );                       /* send last formatted line on its way */
    flags = flags | NOFORMAT;         /* turn no format flag on */
    strcat( obuf, "<pre>" );   /* put in html preformatted tag */
    optr += 5;
    FMflush( );
    break;

   case C_PAGE:             /* eject the page now */
    FMflush( );             /* terminate the line in progress */
    FMpara(  0, 0 );        /* terminate the last line - hard break */
 /* FMpflush( );      */    /* and do the flush */
    break;

   case C_PAGEMAR:            /* number of cols to shift odd pages for holes */
     FMindent( &pageshift );  /* set up the page margin */
     break;

   case C_PUSH:
   case C_POP:		
		break;

   case C_PUNSPACE:           /* toggle punctuation space */
     flags2 ^= F2_PUNSPACE;
     break;

   case C_PAGENUM:                /* adjust page number flag */
    FMgetparm( &buf );            /* get and ignore the parameter */
    break;                        /* as its not supported in hfm */

   case C_QUIT:               /* stop everything now */
    AFIclose( fptr->file );   /* by shutting the input file */
    break;

   case C_RFOOT:              /* define running footer */
    boty = DEF_BOTY - ((rhead == NULL) ? 55 : 80);
    FMsetstr( &rfoot, HEADFOOT_SIZE );
    FMrunset( );              /* output footer information */
    if( rfoot != NULL )
     boty -= (flags & PAGE_NUM) ? 15 : 25;   /* decrease page size */
    else                                     /* amount depends on page num */
     boty += (flags & PAGE_NUM) ? 15 : 25;   /* increase page size */
    break;

   case C_RHEAD:              /* define running header */
    boty = DEF_BOTY - ((rfoot == NULL || !(flags & PAGE_NUM)) ? 55 : 80);
    FMsetstr( &rhead, HEADFOOT_SIZE );
    FMrunset( );              /* output new header information */
    if( rhead != NULL )
     boty -= 25;              /* reduce page size by 25 points */
    else
     boty += 25;              /* header gone - increase page size */
    break;

   case C_RIGHT:              /* right justify the line */
 /* FMright( );   */
    break;

   case C_RSIZE:                  /* set running head/foot font size */
    if( FMgetparm( &buf ) > 0 )   /* get the parameter */
     {
      runsize = atoi( buf );      /* convert to integer */
      if( runsize > 10 )
       linesize = 10;             /* dont allow them to be crazy */
     }
    break;

   case C_SETX:               /* set x position in points */
  /*FMsetx( ); */
    break;

   case C_SETY:               /* set the current y position */
/*  FMsety( );   */          /* get parm and adjust cury */
    break;

   case C_SHOWV:              /* show variable definition to stdout */
    if( (len = FMgetparm( &buf ) ) > 0 )   /* get the variable to look for */
     {
	if( strcmp( buf, "all" ) == 0 )
		FMshowvars( );
	else
		if( buf = sym_get( symtab, buf, 0 ) )
      			FMmsg( -1, buf ); 
		else
			FMmsg( -1, "undefined varaiable" );
     }
    break;

   case C_SINGLESPACE:        /* turn off double space */
    if( flags & DOUBLESPACE )
     flags = flags & (255-DOUBLESPACE);
    break;

   case C_SKIP:             /* put blank lines in if not at col top */
  /*if( cury == topy )  */
     break;                 /* if not at top fall into space */

   case C_SPACE:            /* user wants blank lines */
    FMspace( buf );
    break;

   case C_SECTION:           /* generate rtf section break */
 /* FMsection( );    */
    break;

   case C_SETFONT:           /* set font for text (font name only parm) */
     FMflush( );
     if( (len = FMgetparm( &ptr )) != 0 );     /* if a parameter was entered */
      {
       if( curfont )
        free( curfont );          /* release the current font string */
       curfont = (char *) malloc( (unsigned) (len+1) );  /* get buffer */
       flags2 |= F2_SETFONT;           /* need to change font on next write */
       strncpy( curfont, ptr, len );   /* save the font we set things to */
       curfont[len] = EOS;             /* terminate the string */
      }
     break;

   case C_SETTXTSIZE:                 /* set text font size */
     FMflush( );
     if( FMgetparm( &ptr ) != 0 )     /* if number entered */
      {
       if( (i = atoi( ptr )) >= 4 )    /* if number is ok */
        {
         flags2 |= F2_SETFONT;        /* need to change font on next write */
         textsize = i/4;              /* save the size for future reference */
        }
      }
     break;

   case C_TABLE:                        /* generate start table tags */
    FMtable( );
    break;

   case C_TABLEROW:                           /* force table to next row */
    FMtr( );
#ifdef KEEP 
    FMflush( );                               /* empty the cache */
    curcell = 0;
    AFIwrite( ofile, "</td></tr><tr>" );
    FMcell( 0 );                               /* start 1st cell */
    flags2 |= F2_SETFONT;           /* need to change font on next write */
#endif
    break;

   case C_TMPTOP:            /* set topy to cury for rest of page */
/*  FMtmpy( cmd ); */        /* save/restore topy */
    break;

   case C_TOC:               /* table of contents command */
    FMtc( );                 /* process the command */
    break;

   case C_TOPMAR:            /* set the top margin */
    FMtopmar( );
    break;

   case C_TWOSIDE:          /* toggle two side option flag */
    flags2 = flags2 ^ F2_2SIDE;
    break;

   case C_TOUPPER: 		/* translate to upper; n chars of next tok */
    if( FMgetparm( &buf ) > 0 )   /* get the parameter */
     {
      xlate_u = atoi( buf );
     }
    else
     xlate_u = 1;
    break;

			case C_TRACE:	if( FMgetparm( &ptr ) != 0 )
						trace = atoi( ptr );
					else
						trace = 0;
					break;

     default:
     FMmsg( I_UNKNOWN, buf );
     break;
    }     /* end switch for k - z */
  }       /* end else */

 return 0;
}                  /* FMcmd */
