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
*  Mnemonic: FMcomma
*  Abstract: take the next token and add commas assuming it is a number:
*		12345678 ==> 12,345,678
*  Returns:  Pushes token back onto the stack
*  Date:     26 Feb 2007
*  Author:   E. Scott Daniels
*
*  Modified: 
****************************************************************************
*/
void FMcomma( )
{
	char *buf;               /* parameter pointer */
	char	*p;
	int len;                 /* length of the parameter */
	int i;                   /* index vars */
	int j;
	int n;			/* number of commas needed */
	int nxt;		/* number of chars before next comma */
	char wbuf[1024];

	len = FMgetparm( &buf );     
	if( len <= 0 )
		return;                     /* no parm, exit now */

	if( (p = index( buf, '.' )) )
	{
		n = ((p-buf)-1)/3;
		nxt = (p-buf) - (n * 3);
	}
	else
	{
		n = (len-1) / 3;
		nxt = len - (n * 3);
	}

	j = 0;
	for( i = 0; i < len; i ++)
	{
		wbuf[j++] = buf[i]; 
		if( --nxt == 0 && n )
		{
			n--;
			nxt = 3;
			wbuf[j++] = ',';
		}
	}

	wbuf[j] = 0;
	AFIpushtoken( fptr->file, wbuf );   /* push the command on input stack */

	
}                         
