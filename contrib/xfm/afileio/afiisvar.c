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
/*
*---------------------------------------------------------------------------
* Mnemonic: afiisvar
* Abstract: This routine will check a token to see if there is an 
*           un escaped variable in the token. It will return the addr
*           of the variable symbol if there is.
* Parms:    tok - pointer into tmb->ilist of start of token
*           tmb - Pointer to token management block
* Returns:  Pointer to first vc found or NULL.
* Date:     13 October 1997
* Author:   E. Scott Daniels
*
*---------------------------------------------------------------------------
*/
#include "afisetup.h"

char *AFIisvar( char *tok, struct tokenmgt_blk *tmb )
{
	char *vtok;                /* pointer at variable character in token */

	for( vtok = tok; vtok < tmb->ilist->end; vtok++ )
		if( *vtok == tmb->escsym )
			vtok++;				/* skip next - its escaped */
		else
			if( *vtok == tmb->varsym )
				return( vtok );
	return NULL;
}

#ifdef KEEP
char *AFIisvar( char *tok, struct tokenmgt_blk *tmb )
{
 char *vtok;                /* pointer at variable character in token */

 for( vtok = tok; vtok < tmb->ilist->end && *vtok != tmb->varsym; vtok++ )
  if( strchr( tmb->tsep, *vtok ) )
    return( NULL );                     /* end of token before var */
  

 if( vtok < tmb->ilist->end && 
     (vtok == tok  || *(vtok-1) != tmb->escsym) )     /* not escaped */
  return( vtok );

  return( NULL );
}
#endif

