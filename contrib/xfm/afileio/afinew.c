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
*************************************************************************
*
*  Mnemonic: AFInew
*  Abstract: This routine is the central creation point for structures
*            used by the AFI routines.
*  Parms:    type - Type of structure that is to be allocated and
*                   initialized.
*  Returns:  The pointer to the structure, or NULL if an allocation error
*  Date:     28 March 1997
*  Author:   E. Scott Daniels
*  Mods:
*		10 Apr 2007 - Memory leak cleanup
*
**************************************************************************
*/

#include "afisetup.h"

void *AFInew( int type )
{
 struct inputmgt_blk *iptr;   /* pointers of things we can allocate */
 struct tokenmgt_blk *tptr;
 void *rptr = NULL;           /* pointer for return value */


 switch( type )
  {
   case INPUTMGT_BLK:
     iptr = (struct inputmgt_blk *) malloc( sizeof( struct inputmgt_blk ));
     if( iptr != NULL )
      {
	memset( iptr, 0, sizeof( *iptr ) );
       iptr->flags = 0;
       iptr->next = NULL;
       rptr = (void *) iptr;           /* convert for return */
      }
     break;

   case TOKENMGT_BLK:
     tptr = (struct tokenmgt_blk *) malloc( sizeof( struct tokenmgt_blk ) );
     if( tptr != NULL )
      {
	memset( tptr, 0, sizeof( *tptr ) );
       tptr->varsym = (char) -1;        /* default to something not likely */
       tptr->escsym = (char) -1;
       tptr->psep = strdup( ":" ); 
       tptr->ilist = NULL;
       rptr = (void *) tptr;
      }
     break;
  }                       /* end switch */

 return( rptr );

}                       /* AFInew */
