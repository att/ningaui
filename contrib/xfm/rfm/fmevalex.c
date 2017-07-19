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
*  Mnemonic: FMevalex
*  Abstract: This routine will evaluate a reverse polish expression and
*            return the character equivalent of the result. The
*            expression may contain constant and already defined variables.
*            Expressions have the following format:
*               [ &var &var + ]   (Add var to var)
*  Returns:  The integer result of the expression
*  Parms:    Optional - buf - Pointer to buffer to convert result into
*  Date:     28 March 1993
*  Author:   E. Scott Daniels
*
*  Modified: 21 Apr 1997 - To calc in floating point
*            22 Apr 1997 - To truncate trailing 0s from string.
*            10 Jun 1997 - To add TIME command
*                          To add FRAC command
*                          To add INT command
**************************************************************************
*/
int FMevalex( rbuf )
 char *rbuf;
{
 int i;             /* integer value */
 int sp = 0;        /* stack pointer */
 char *idx;         /* pointer into string */
 double stack[100]; /* stack */
 char *buf;         /* pointer at next token to process */
 char fmt[100];     /* sprintf format string */
 int usr_fmt = 0; 

 sprintf( fmt, "%%.4f" );   /* default format string */

 stack[0] = 0;

while( FMgetparm( &buf ) ) 
  {
   switch( *buf )
    {
     case 'T':                      /* convert top of stack to time */
      i = stack[sp-1];              /* convert top of stack to integer */
      stack[sp-1] = i + ((stack[sp-1] - i) * 0.60);
      break;

     case 'I':                      /* make top integer */
      i = stack[sp-1];
      stack[sp-1] = i;
      break;

     case 'F':                      /* leave fractial portion of top */
      i = stack[sp-1];
      stack[sp-1] = stack[sp-1] - i;
      break;

     case 'S':                      /* sum all elements on the stack */
      while( sp > 1 )
       {
        stack[sp-2] += stack[sp-1];   /* add top two and push */
        sp--;                         /* one less thing on stack */
       }
      break;
       
     case '+':
	if( isdigit( *(buf+1) ) )
      		stack[sp++] = atof( buf );  /* convert to integer and push on stack */
	else
	{
if( trace )
fprintf( stderr, "trace: [%.3f %.3f +] = ", stack[sp-2], stack[sp-1] );
      		stack[sp-2] += stack[sp-1];   /* add top two and push */
      		sp--;                         /* one less thing on stack */
if( trace )
fprintf( stderr, "%.3f\n", stack[sp-1] );
	}
	break;

     case '-':
	if( isdigit( *(buf+1) ) )
      		stack[sp++] = atof( buf );  /* convert to integer and push on stack */
	else
	{
      		stack[sp-2] = stack[sp-2] - stack[sp-1];   /* sub top two and push */
      		sp--;                         /* one less thing on stack */
	}
	break;

     case '*':
      stack[sp-2] *= stack[sp-1];   /* mul top two and push */
      sp--;                         /* one less thing on stack */
      break;

     case '/':
      if( stack[sp-1] != 0 )
       stack[sp-2] = stack[sp-2]  / stack[sp-1];   /* div top two and push */
      sp--;                         /* one less thing on stack */
      break;

     case '%':                           /* mod operator */
      stack[sp-2] = (int) stack[sp-2] % (int) stack[sp-1];
      sp--;
      break;

     case '?':
      usr_fmt++;
      strcpy( fmt,  buf+1 );
      break;

     case ']':
      if( rbuf != NULL )                      /* if caller passed a buffer */
       sprintf( rbuf, fmt, stack[sp-1] );  /* convert to character */
      for( idx = rbuf + strlen( rbuf ) -1; *idx == '0'; idx-- );  
      if( ! usr_fmt )
       {
        if( *idx == '.' )
         *idx = EOS;                       /* no fractional amt, cut it out */
        else
         idx[1] = EOS;                    /* leave last non 0 there */
       }
      return( (int) stack[sp-1] );                /* return top of stack */
      break;

     default:               /* assume its a parameter to push on stack */
      stack[sp++] = atof( buf );  /* convert to integer and push on stack */
      break;
    }     /* end switch */
  }       /* end while */

                        /* if we fall out of the while then return top value */
 if( rbuf != NULL )  /* if caller passed a buffer */
  sprintf( rbuf, "%d", stack[sp-1] );  /* convert to character */
}           /* FMevalex */
