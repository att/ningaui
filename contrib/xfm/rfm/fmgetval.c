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
*  Mnemonic: FMgetval
*  Abstract: This routine is invoked when the .gv command is encountered in
*            the input file. It creates a psuedo command in the input buffer
*            which creates a variable using the .dv command based on the
*            parameter that was passed in with the .gv command. The .gv
*            command must be entered on a line by itself.
*            The following are valid parameters:
*              D  	- Set variable _date to current dd <string> yyyy
*              d  	- Set variable _date to the current m/d/y date
*              e  env-name xfm-var-name - set xfm-var-name to environment var name env-name
*              fi 	- Set variable _fig to the current figure number
*              fo 	- Set variable _font to the current font string
*              h  	- set variable _host to the local host name
*		li[nes] - set variable _line to lines remaining in the current col.
*              lm  	- Set variable _lmar to current left margin value
*		mdy 	- Set variables _mon _day _year with current date values
*              p  	- Set variable _page to the current page number
*              rm[ar]  	- Set variable _rmar to current right margin
*		re[main] - Set the variables _iremain _premain _lines for the inches/points/lines remaining in current col
*		s[ect] 	- Set the var _sect to the current h1 value
*              t  	- Set variable _tsize to current text point size
*              T  	- Set variable _time to current hours and minutes
*	       v  	- XFM/HFM/RFM version number
*              y  	- Set variable _cury to the current y value
*  Returns:  Nothing.
*  Date:     6 April 1993
*  Author:   E. Scott Daniels
*
*  Modified: 26 Mar 1997 - To correct problem with page number get
*             3 Apr 1997 - To use the tokenizer now in AFI!
*            28 Apr 2001 - To add e (environment snag) 
******************************************************************************
*/
/*
#include <sys/systeminfo.h>
*/

char *mname[13] = {
 " ", "January", "February", "March", "April", "May", "June", "July", 
 "August", "September", "October", "November", "December" 
};

void FMgetval( )
{
	char *buf;             /* pointer to parameter to use */
	char *ep;		/* pointer to value of env var */
	char *ename;           /* pointer to environment var name */
	char work[128];
	char cmd[2048];        /* buffer to build .dv commands in */
	int m;                 /* parameters to get date/time in */
	int d;
	int y;
	int h;
	int s;
	int i;
	struct col_blk *cp;

 *cmd = 0;

 if( FMgetparm( &buf ) > 0 )  /* if there is a parameter on the line */
  {
   iptr = 0;           /* start the input pointer at beginning of buffer */
   switch( *buf )      /* look at user parameter and set psuedo command */
    {
     case 'D':         /* get string formatted date */
       UTmdy( &m, &d, &y );   /* get the values */
       sprintf( cmd, ".dv _date %d %s %d : ", d, mname[m], 1900 + y );
       break;

     case 'd':         /* set date value */
       UTmdy( &m, &d, &y );   /* get the values */
       sprintf( cmd, ".dv _date %d/%d/%d : ", m, d, y );  /* create cmd */
       break;

     case 'e':		/* e env-name xfm-var-name */
	if( FMgetparm( &buf ) > 0 )  /* if there is a parameter on the line */
	{
		ename = strdup( buf );

		if( FMgetparm( &buf ) > 0 )
		{
			if( ep = getenv( ename ) )
				sprintf( cmd, ".dv %s %s : ", buf, ep );
		}

		free( ename );
	}
	break;
	

     case 'f':                      /* set figure number or font variable */
       if( *(buf+1) == 'i' )
	{
		if( flags & PARA_NUM )
			sprintf( cmd, ".dv _fig %d-%d : ", pnum[0], fig );
		else
			sprintf( cmd, ".dv _fig %d : ", fig );
	}
       else
        sprintf( cmd, ".dv _font %s : ", curfont );
       break;

     case 'h':                       /* get host name */
       gethostname( work, 128 );
       sprintf( cmd, ".dv _host %s : ", work );
       break;

     case 'l':      /* set margin variables (lmar) or lines remaining in col (lines) */
	if( *(buf+1) == 'i' )
		sprintf( cmd, ".dv _lines %d : ", (boty - cury)/(textsize + textspace) );
	else
		sprintf( cmd, ".dv _lmar %d : ", lmar );
       break;

     case 'm':         					/* set month day year */
       UTmdy( &m, &d, &y );   				/* get the values */
       sprintf( cmd, ".dv _mon %s : .dv _day %d : .dv _year %d : ", mname[m], d, 1900 + y );
       break;

     case 'p':       /* set page variable */
       sprintf( cmd, ".dv _page %d : ", page+1 );
       break;

     case 'r':       /* set right margin value */
	if( *(buf+1) == 'e' )		/* assume remain - generate iremain (inches) lines */
	{
		sprintf( cmd, ".dv _lines %d : ", (boty - cury)/(textsize + textspace) );
     		AFIpushtoken( fptr->file, cmd );   
		sprintf( cmd, ".dv _iremain %d : ", (boty - cury)/72 );		/* 72 points per inch */
     		AFIpushtoken( fptr->file, cmd );   
		sprintf( cmd, ".dv _premain %d : ", (boty - cury));		/* points */
	}
	else
       		sprintf( cmd, ".dv _rmar %d  : ", lmar + linesize );
       break;
	
	case 's':
		sprintf( cmd, ".dv _sect %d", pnum[0] );
		break;


     case 't':       /* set current text size */
       sprintf( cmd, ".dv _tsize %d  : ", textsize );
       break;

     case 'T':        /* set current time */
       UThms( &h, &m, &s );   /* get the values */
       sprintf( cmd, ".dv _time %02d:%02d : ", h, m );
       break;

     case 'v':
	sprintf( cmd, ".dv _ver %s", version );
	break;

     case 'y':       				/* set current y info: current y, top y, col-number  */
	sprintf( cmd, ".dv _cury %d : ", cury );
     	AFIpushtoken( fptr->file, cmd );  
	sprintf( cmd, ".dv _topy %d : ", topy );
     	AFIpushtoken( fptr->file, cmd );  
	i = 0;
	for( cp = firstcol; cp && cp != cur_col; cp = cp->next )
		i++;
	sprintf( cmd, ".dv _coln %d : ", i );
       break;

     default:
       FMmsg( E_PARMOOR, inbuf );   /* error message */
       break;
    }       /* end switch */

	if( *cmd )
     		AFIpushtoken( fptr->file, cmd );   /* push the command on input stack */
  }                                       /* end if parameter entered */
}          /* fmgetval */
