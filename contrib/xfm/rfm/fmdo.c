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
* --- NOTE: This never worked cleanly, I have disabled it with the intro
* ---	of the symboltable variables as it needs to be rewritten
*
*  Mnemonic: FMdo
*  Abstract: This routine sets things up to execute a string of commands
*            until a condition is met.
*            .do stop == 0 .im nextfile 
*  Parms:    None.
*  Returns:  Nothing.
*  Date:     9 October 1998
*  Author:   E. Scott Daniels
*
*  The proper for loop where $1 is var name, $2 is start value, $3 is 
*  the condition, $4 is the command string to execute:
*  .dv for .dv $1 $2 ^: .do $1 $3 $5 .dv $1 ^^[ ^^&$1 $4 ]
*
*  Modified: 	13 Jan 2002 - To use symboltable for vars
****************************************************************************
*/
void FMdo( )
{
	return;

#ifdef KEEP
 char *buf;               /* parameter pointer */
 char tbuf[1024];
 char docmd[2048];        /* do command to push on the input stack */
 struct var_blk *vp;      /* pointer at variable to check */
 int len;                 /* length of the parameter */
 int i;                   /* index vars */
 int j;
 int k;
 char ebuf[5];
 int result = 0;          /* true if we can process command string */

 sprintf( docmd, ".do " );    /* initialise the do command buffer */

 len = FMgetparm( &buf );     /* get the stop expression token 1 */
 if( len <= 0 )
  {
   fprintf( stderr, ".do: missing paramters" );
   return;                     /* no name entered then get out */
  }

 if( strncmp( "error", buf, 5 ) != 0 )
  {
   len = len > MAX_VARNAME ? MAX_VARNAME : len;   /* truncate at max */
   strncpy( tbuf, buf, len );   /* get the name to look for */
   tbuf[len] = EOS;              /* make it a real string */

   strcat( docmd, tbuf );

   for( vp = varlist; vp != NULL  && strcmp( vp->name, tbuf ) != 0;
        vp = vp->next );

   if( vp )
    {
     len = FMgetparm( &buf );    /* == >= <= != > or < */
     strncpy( ebuf, buf, len );
     *(ebuf+len) = 0;

     strcat( docmd, " ");
     strcat( docmd, ebuf );


     if( (len = FMgetparm( &buf )) )
      {
       strncpy( tbuf, buf, len );
       *(tbuf+len) = 0;
       strcat( docmd, " ");
       strcat( docmd, tbuf );

       i = atoi( tbuf );
       j = atoi( vp->string );

       switch( *ebuf )
        {
         case '=' :
          result = i == j ; 
          break;
         case '<' :
          if( *(ebuf+1) == '=' )
           result = j <= i;
          else
           result = j < i;
          break;
         case '>' :
          if( *(ebuf+1) == '=' )
           result = j >= i;
          else
           result = j > i;
          break;
         case '!' :
          result = i != j;
          break;
        }
      }    /* end if value parm was there */
     else
      fprintf( stderr, ".do variable defined but has no value: %s\n", tbuf );
    }
   else
    fprintf( stderr, ".do variable not defined: %s\n", tbuf );
  }

 i = j = 0;              /* prep to load the new expansion information */

 strcat( docmd, " ");
 k = strlen( docmd );
 while( (len = FMgetparm( &buf )) > 0 )   /* get all parms on cmd line */
  {
   for( i = 0; i < len && k < 1024; i++ )       /* ensure specal syms */
    {                                           /* are preserved */
     if( *(buf+i) == '&' )
      docmd[k++] = '^';
     docmd[k++] = buf[i];
    }
   docmd[k++] = ' ';

   for( i = 0; i < len && j < MAX_VAREXP; i++, j++ )
    {
     if( buf[i] == '^' )                      /* remove escape symbol */
      switch( buf[i+1] )                      /* only if next char is special*/
       {
        case ':':
        case '`': 
        case '[':             
         i++;                              /* skip to escaped chr to copy */  
         break;

        default:                              /* not a special char - exit */
         break;
       }                                      /* end switch */
      tbuf[j] = buf[i];                        /* copy in the next char */
    }
   tbuf[j++] = BLANK;          /* seperate parms by a blank */
  }

 j = j > 0 ? j - 1 : j;    /* set to trash last blank if necessary */
 tbuf[j] = EOS;             /* and do it! */
 docmd[k?k-1:0] = 0;

 if( result )        /* continue with execution? */
  {
   strcat(docmd, " :" );
   strcat(tbuf, " :" );
   AFIpushtoken( fptr->file, docmd );    /* push another do command */
   AFIpushtoken( fptr->file, tbuf );     /* push to execute command string */
  }

#endif
}                          /* FMdo */
