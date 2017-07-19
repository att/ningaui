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
***************************************************************************
*
*   Mnemonic: FMcmd
*   Abstract: This routine is responsible for dispatching the proper
*             routine to handle the command that was input by the user.
*             Commands are those tokins that begin with a period and are 2
*             characters long.
*   Parms:    buf - Pointer to the command. (.aa)
*   Returns:  Nothing.
*   Date:     17 November 1988
*   Author:   E. Scott Daniels
*
*   Modified: 1-1-89 - To support normal and definition lists
*             4-30-98- To support user defined list item characters
*             5-05-89- To support boxes
*             6-10-89- To add .** as a comment line in the input file
*             3 May 1991 - To support .ts (two sided) command
*             4 May 1991 - To support page shifting
*             5 May 1992 - To support postscript
*             1 Oct 1992 - To support punctuation space command
*             2 Nov 1992 - To add center command
*             7 Nov 1992 - To split code into two sub modules fmcmd1.c and
*                          fmcmd2.c
*            21 Feb 1994 - To handle shift value in dlstack rather than old
*                          left margin; correct multi column problem
*            14 Jan 2000 - Added block center capability as it is great in HFM
*		13 Nov 2007 - Added runstop support
**************************************************************************
*/
int FMcmd( buf )
char *buf;
{
	int cmd;                  /* command converted to integer for switch */
	int i;                    /* temp integer */
	struct li_blk *liptr;     /* pointer to head to delete on a .el cmd */
	char *ptr;                /* dummy pointer to skip parameters */
	int len;                  /* length of parameter returned */

	cmd = toupper( buf[1] );
	i = toupper( buf[2] );       /* pick off and translate indiv characters */
	cmd = (cmd << 8) + i;        /* combine the characters */

TRACE( 1 ) "rfm cmd %s\n", buf  );
	switch( cmd )
	{

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

			case C_BLOCKCTR:                    /* block center */
				FMgetparm( &ptr );                 /* valid are start/end */
				FMflush( );
				if( *ptr == 's' || *ptr == 'S' )   /* assume start */
					flags2 |= F2_CENTER;              /* force flush to center it */
				else
					flags2 &= ~F2_CENTER;             /* turn it off */
				break;

			case C_BREAK:            /* user wants a line break */
				FMflush( );
				if( (rflags & RF_PAR) == 0 )   /* if not already broken */
					FMpara( 0, TRUE );            /* force a new paragraph mark */
				break;

			case C_BOX:              /* start or end a box  */
				FMbox( );               /* start or end the box */
				break;

			case C_BOTY:            /* set bottom of page y value */
				FMgetparm( &ptr );                 /* valid are start/end */
				boty = atoi( ptr );
				break;
		
			case C_CAPTURE:			/* capture lines to a file */
				FMcapture( );
				break;

			case C_CCOL:             /* conditional column eject */
				FMccol( 0 );            /* 0 parameter says to get from input */
				break;

			case C_CENTER:           /* center text that follows the .ce command */
				FMcenter( );
				break;
	
			case C_TABLECELL:         /* go to next table cell */
				FMflush( );
				while( FMgetparm( &ptr ) != 0 );    /* skip all parms on the line */
				AFIwrite( ofile, "\\cell" );
				break;

			case C_COMMA:
				FMcomma( );
				break;

			case C_COMMENT:         /* ignore the rest of the input line */
				while( FMgetparm( &ptr ) != 0 );    /* skip all parms on the line */
				break;

			case C_DEFHEADER:        /* define header attributes */
				FMdefheader( );
				break;

			case C_CDEFINE:          /* define column mode */
				FMcd( );
				FMsetmar( );            /* set the margins based on current column */
				break;

			case C_ELSE:             /* .ei else part encountered */
				FMelse( );
				break;

			case C_EP:               /* embedded postscript - just copy in rtf file */
				/*  FMep( );  */            /* not supported in rfm */
				break;

			case C_CEJECT:           /* start new column */
				FMflush( );             /* flush what is there... must be done here */
				FMceject( 1 );            /* eject column, flush page if in last column */
				break;

			case C_CPAGE:            /* conditional page eject */
				FMcpage( );
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
					FMflush( );             /* flush out last line of definition */
					i = dlstack[dlstackp].indent / PTWIDTH;  /* calc line len shift */
					lmar -= dlstack[dlstackp].indent;        /* shift margin back to left */
					linelen += dlstack[dlstackp].indent;     /* reset line len */
					dlstackp--;                          /* "pop" from the stack */
					FMpara( 0, TRUE );                   /* this is the end of a par */
					cury += textsize + textspace;        /* para causes extra line */
				}
				break;

			case C_ENDIF:            /* .fi encountered - ignore it */
				break;

			case C_ENDLIST:          /* end a list */
				if( lilist != NULL )    /* if inside of a list */
				{
					FMflush( );           /* clear anything that is there */
					FMendlist( TRUE );      /* terminate the list and delete the block */
				}
				break;

			case C_ENDTABLE:         /* end the table */
				FMflush( );             /* flush out last line of definition */
				FMendtable( );
				break;


			case C_EVAL:                  /* evaluate expression and push result */
				if( FMgetparm( &buf ) > 0 )  /* get parameter entered */
				AFIpushtoken( fptr->file, buf );
				break;

			case C_FIGURE:           /* generate figure information */
				FMfigure( );
	
				break;

			case C_FORMAT:           /* turn format on or off */
				FMformat( );
				break;

			case C_GETVALUE:         /* put an FM value into a variable */
				FMgetval( );
				break;

			case C_GREY:             /* set grey scale for fills */
				if( FMgetparm( &buf ) > 0 )  /* get parameter entered */
					fillgrey = atoi( buf );   /* convert it to integer */
				break;

			case C_HDMARG:           /* adjust the header margin */
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
				if( (rflags & RF_PAR) == 0 )  /* previous paragraph not terminated */
					AFIwrite( ofile, "\\par" );  /* so do it */
				FMindent( &lmar );      /* get the users info from input and assign */
				FMpara( 0, FALSE );     /* set up the paragraph with new indention */
				break;

			case C_JUMP:             /* jump to label */
				FMjump( );
				break;

			case C_JUSTIFY:         /* justify command */
				FMsetjust( );       /* causes strange things to happen in rfm so dont */
				break;

		
			case C_LINESIZE:               /* set line size for line command */
				if( FMgetparm( &buf ) > 0 )   /* get the parameter */
				{
					linesize = atoi( buf );     /* convert to integer */
					if( linesize > 10 )
						linesize = 2;              /* dont allow them to be crazy */
				}
				break;

			case C_LISTITEM:         /* list item entered */
				if( lilist != NULL )
					FMlisti( );            /* output what we have so far */
				break;

			case C_LL:               /* reset line length */
				FMflush( );             /* terminate previous line */
				if( (rflags & RF_PAR) == 0 )   /* terminate previous if not already  */
				{
					AFIwrite( ofile, "\\par" );
					rflags |= RF_PAR;            /* indicate this was done */
				}
				FMll( );                /* get and set line size parameters */
				FMpara( 0, FALSE );     /* reset information in the file */
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
	
			case C_NOFORMAT:                /* turn formatting off */
				FMflush( );                    /* send last formatted line on its way */
				flags = flags | NOFORMAT;      /* turn no format flag on */
				break;

			case C_PAGE:             /* eject the page now */
				FMflush( );             /* terminate the line in progress */
				FMpara( 0, TRUE );      /* terminate the last line - hard break */
				FMpflush( );            /* and do the flush */
				break;

			case C_PAGEMAR:            /* number of cols to shift odd pages for holes */
				FMindent( &pageshift );  /* set up the page margin */
				break;
		
			case C_POP:		break;

			case C_PUSH:		break;

			case C_PUNSPACE:           /* toggle punctuation space */
				flags2 ^= F2_PUNSPACE;
				break;

			case C_PAGENUM:            /* adjust page number flag */
				boty = DEF_BOTY - ((rhead == NULL) ? 60 : 85);
				FMpgnum( );
				FMrunset( );
				if( flags & PAGE_NUM )   /* if numbering pages */
					boty -= rfoot == NULL ? 25 : 15;  /* adjust space if on */
				else
					boty += rfoot == NULL ? 25 : 15;  /* adjust space if on */
				break;

			case C_QUIT:               /* terminate processing right now! */
				AFIclose( fptr->file );   /* close the input file */
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
					FMright( );
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
				FMsetx( );
				break;

			case C_SETY:               /* set the current y position */
				FMsety( );                /* get parm and adjust cury */
				break;

			case C_SHOWV:                      		/* show variable definition to stdout */
				if( (len = FMgetparm( &buf ) ) > 0 )   	/* get the variable to look for */
				{
					if( strcmp( buf, "all" ) == 0 )
						FMshowvars( );			/* show all definitions */
					else
					{
						if( (buf = sym_get( symtab, buf, 0 )) )
							FMmsg( -1, buf ); 
						else
							FMmsg( -1, "undefined variable" );
					}
				}
				break;

			case C_SINGLESPACE:        /* turn off double space */
				if( flags & DOUBLESPACE )
				flags = flags & (255-DOUBLESPACE);
				break;

			case C_SKIP:             /* put blank lines in if not at col top */
				if( cury == topy )
					break;                 /* if not at top fall into space */

			case C_SPACE:            /* user wants blank lines */
				FMspace( buf );
				break;

			case C_SECTION:           /* generate rtf section break */
				FMsection( );
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

			case C_SETTXTSIZE:        /* set text font size */
				FMflush( );
				if( FMgetparm( &ptr ) != 0 )     /* if number entered */
				{
					if( (i = atoi( ptr )) > 5 )    /* if number is ok */
					{
						flags2 |= F2_SETFONT;        /* need to change font on next write */
						textsize = i;                /* save the size for future reference */
					}
				}
				break;

			case C_STOPRUN:			/* .sr command to pop the run stack */
				return 2;
				break;

			case C_TABLE:             /* start a new table */
				FMflush( );
				FMtable( );
				break;

			case C_TABLEBRK:          /* safe rtf table break */
				FMflush( );
				AFIwrite( ofile, "\\par" );
				break; 

			case C_TABLEROW:          /* go to next table row */
				FMflush( );
				while( FMgetparm( &ptr ) != 0 );    /* skip all parms on the line */
					FMrow( MIDDLE );
				break;

			case C_TMPTOP:            /* set topy to cury for rest of page */
				FMflush( );
				FMtmpy( cmd );           /* save/restore topy */
				break;

			case C_TOC:               /* table of contents command */
				FMtc( );                 /* process the command */
				break;

			case C_TOPMAR:            /* set the top margin */
				FMtopmar( );
				break;

			case C_TWOSIDE:          /* toggle two side option flag */
				flags2 = flags2 ^ F2_2SIDE;
				AFIwrite( ofile, "\\facingp" );    /* turn on facing page in doc */
				break;

			case C_TOUPPER: 		/* translate to upper; n chars of next tok */
				if( FMgetparm( &buf ) > 0 )   /* get the parameter */
				{
					xlate_u = atoi( buf );
				}
				else
					xlate_u = 1;
				break;

			default:
				FMmsg( I_UNKNOWN, buf );
				break;
		}     

	return 1;
}                  /* FMcmd */
