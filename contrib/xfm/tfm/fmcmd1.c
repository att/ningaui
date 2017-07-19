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


/* TFM */
   case C_ABORT:            /* immediate exit */
    if( FMgetparm( &buf ) > 0 )  /* get parameter entered */
     exit ( atoi( buf ) ); 
    else
     exit( 0 );
    break;

   case C_ASIS:             /* begin reading input as is */
    flags2 |= F2_ASIS;      /* turn on asis flag */
    break;

   case C_BEGDEFLST:        /* begin a definition list */
    FMbd( );                /* start the list */
    break;

   case C_BEGLIST:          /* begin a list */
    FMbeglst( );            /* set up for a list */
    break;

   case C_BREAK:            /* user wants a line break */
    FMflush( );
    break;

   case C_BLOCKCTR:         /* start/end centering a block of text */
    FMgetparm( &ptr );      /* get and ignore the parameter */
    FMflush( );
    break;

   case C_BOX:              /* start or end a box  */
    /*FMbox( ); */              /* start or end the box */
    FMignore( );
    break;

   case C_COLOR:
    if( FMgetparm( &ptr ) != 0 )
     {
      FMflush( );         /* put out what was collected in prev colour */
      if( textcolour )
       free( textcolour );
      textcolour = strdup( ptr );
      flags2 |= F2_SETFONT;                /* need to set a new font */
     }
    break;

   case C_CCOL:             /* conditional column eject */
    FMccol( 0 );            /* 0 parameter says to get from input */
    break;

   case C_CENTER:           /* center text that follows the .ce command */
    FMcenter( );
    break;

   case C_TABLECELL:       /* .cl new table cell */
    /*FMcell( 1 ); */
    FMignore( );
    break;

   case C_COMMENT:         /* ignore the rest of the input line */
    FMskip( );
    break;

   case C_DEFHEADER:        /* define header attributes */
    FMdefheader( );
    break;

   case C_CDEFINE:          /* define column mode */
    FMcd( );
 /* FMsetmar( );   */       /* set the margins based on current column */
    break;

   case C_ELSE:             /* .ei else part encountered */
    FMelse( );
    break;

   case C_EP:               /* embedded postscript - just copy in rtf file */
    break;                  /* not supported */

   case C_CEJECT:           /* start new column */
    FMflush( );             /* flush what is there... must be done here */
  /*FMceject( );   */       /* eject column, flush page if in last column */
    break;

   case C_ENDTABLE:         /* end the table */
#ifdef NOT_ME
    FMflush( );
    if( ts_index )
      ts_index--; 
    if( table_stack[ts_index] )
     {
      if( table_stack[ts_index]->oldfg )
       textcolour = table_stack[ts_index]->oldfg;
      if( table_stack[ts_index]->fgcolour )
       free( table_stack[ts_index]->fgcolour );
      if( table_stack[ts_index]->bgcolour )
       free( table_stack[ts_index]->bgcolour );
   
      free( table_stack[ts_index]->cells );
      free( table_stack[ts_index] );
      table_stack[ts_index] = 0;
      if( ts_index )
       {
        tableinfo =  table_stack[ts_index - 1]->cells;
        curcell = table_stack[ts_index - 1]->curcell;
       }
      else
       tableinfo = 0;
     }
    AFIwrite( ofile, "</td></tr></table>" );    /*end everything */
#endif

    break;

   case C_CPAGE:            /* conditional page eject */
/*  FMcpage( );      */   /* not supported in hfm */
    break;

   case C_DEFDELIM:         /* define the variable definition delimiter */
    if( FMgetparm( &ptr ) != 0 )
     vardelim = ptr[0];    /* set the new pointer */
    break;

   case C_DEFITEM:          /* definition list item */
    FMditem( );
    break;

   case C_DEFVAR:           /* define variable */
    FMdv( );
    break;
 
   case C_DOLOOP:
    FMdo( );
    break;

   case C_DOUBLESPACE:      /* double space command */
    flags = flags | DOUBLESPACE;
    break;

   case C_ENDDEFLST:        /* end definition list */
    if( dlstackp >= 0 )     /* valid stack pointer? */
     {
      flags2 &= ~F2_DIRIGHT;  /* turn off the right justify flag for di's */
      FMflush( );                       /* flush out last line of definition */
      lmar -= dlstack[dlstackp].indent;  /* back to orig lmar */
      linelen += dlstack[dlstackp].indent;  /* back to orig  */
      optr += 8;

      dlstackp--;                          /* "pop" from the stack */
      FMpara( 0, 0 );                      /* this is the end of a par */
      FMflush( );                       /* flush out last line of definition */
      flags2 |= F2_SETFONT;                /* need to set a new font */
     }
    break;

   case C_ENDIF:            /* .fi encountered - ignore it */
    break;

   case C_ENDLIST:          /* end a list */
    if( lilist != NULL )    /* if inside of a list */
     {
      FMflush( );           /* clear anything that is there */
      FMendlist( TRUE );      /* terminate the list and delete the block */
      free( lilist );
      lilist = NULL;

      linelen += 14;
      lmar -= 14;            /* pop out */
     }
    break;

   case C_EVAL:                  /* evaluate expression and push result */
    if( FMgetparm( &buf ) > 0 )  /* get parameter entered */
     AFIpushtoken( fptr->file, buf );
    break;

   case C_FIGURE:           /* generate figure information */
    FMfigure( );

    break;

   case C_FORMAT:           /* turn format on or off */
    if( (flags & NOFORMAT ) && ! (flags2 & F2_ASIS ) )  /* in nf mode but  */
     {                                                  /* not in as is mode */
      FMflush( );                                 /* flush what was started */
     }
    FMformat( );                /* act on user request - set/clr flags */
    break;

   case C_GETVALUE:         /* put an FM value into a variable */
    FMgetval( );
    break;

   case C_GREY:                  /* set grey scale for fills */
    if( FMgetparm( &buf ) > 0 )  /* get parameter entered */
     fillgrey = atoi( buf );     /* convert it to integer */
    break;

   case C_HDMARG:            /* adjust the header margin */
     FMindent( &hlmar );     /* change it */
     break;

   case C_HN:               /* header number option */
    FMhn( );                /* this command is not standard script */
    break;

   case C_H1:               /* header one command entered */
    FMheader( headers[0] ); /* put out the proper header */
    break;

   case C_H2:               /* header level 2 command entered */
    FMheader( headers[1] );
    break;

   case C_H3:               /* header three command entered */
    FMheader( headers[2] ); /* put out the proper header */
    break;

   case C_H4:               /* header level 4 command entered */
    FMheader( headers[3] );
    break;

   case C_IF:               /* if statement */
    FMif( );                /* see if we should include the code */
    break;

   case C_IMBED:            /* copy stuff from another file */
    FMimbed( );
    break;

   case C_INDENT:           /* user indention of next line */
    FMflush( );             /* force a break then */
    FMindent( &lmar );      /* get the users info from input and assign */
    FMpara( 0, 0 );         /* set up the paragraph with new indention */
    break;

   case C_JUMP:
    FMjump( );              /* jump to a label token */
    break;

   case C_JUSTIFY:         /* justify command */
    FMsetjust( );       /* causes strange things to happen in rfm so dont */
    break;

     default:
      FMmsg( I_UNKNOWN, buf );
      break;
    }     /* end switch for commands .a - .j */
  }       /* end if */
 else     /* command is .k - .z */
  {
   switch( cmd )
    {

/* *** continued in cmd2.c file *** */
