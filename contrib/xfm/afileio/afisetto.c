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
***************************************************************************
*
*  Mnemonic: AFIsettoken (afisetto.c)
*  Abstract: This routine is reponsible for conveting the open AFI file 
*            to a tokenized file. After conversion, calls to AFI read
*            will return only the next token from the input stream to 
*            the caller. The caller may also supply a variable symbol
*            which is used to identify and expand variables encountered
*            in the token stream and an escape symbol which can be used
*            to escape special symbols. 
*  Parms:    file   - the AFI file handle of the file to convert
*            tsep   - Pointer to characters that define the start of a new
*                     token and are returned when encountered as a single
*                     character token.
*            varsym - The character used to indicate a variable to try to 
*                     expand.
*            escsym - The character used to force the next character to 
*                     be ignored by the tokenizer.
*            psep   - Parameter seperation character string for UTformat calls 
*            findvar- Pointer to user routine that can be called to look
*                     up a variable name and returns a buffer of characters
*                     that the variable expands to.
*            finddata-Pointer to data that will be passed to the find routine
*  Returns:  AFI_OK if all went well, AFI_ERROR if there was a problem
*  Date:     28 March 1997
*  Author:   E. Scott Daniels
*
* Modified: 	11 Jan 2002 - to add new parse/expansion with symbol table
*		10 Apr 2007 - Fixed memory leak.
***************************************************************************
*/

#include "afisetup.h"

int AFIsettoken( AFIHANDLE file, Sym_tab *st, char *tsep, char varsym, char escsym, char *psep )
{
 int status = AFI_ERROR;             /* status of processing here */
 struct tokenmgt_blk *tptr;          /* pointer at token management block */
 struct file_block *fptr;            /* pointer at file block */

 if( (fptr = afi_files[file]) != NULL )  /* valid file */
  {
	if( st )
		fptr->symtab = st;

   if( fptr->tmb == NULL )           /* no token block yet */
    fptr->tmb = (struct tokenmgt_blk *) AFInew( TOKENMGT_BLK );

   if( fptr->tmb != NULL )           /* allocated successful or there */
    {
	
	if( fptr->tmb->psep )
		free( fptr->tmb->psep );
	fptr->tmb->psep = strdup( psep );

	if( fptr->tmb->tsep )
		free( fptr->tmb->tsep );
     fptr->tmb->tsep = strdup( tsep );

     fptr->tmb->varsym = varsym;
     fptr->tmb->escsym = escsym;       /* fill out with user stuff */

     if( fptr->tmb->ilist == NULL )        /* need an ilist? */
      fptr->tmb->ilist = (struct inputmgt_blk *) AFInew( INPUTMGT_BLK );

     if( fptr->tmb->ilist != NULL )
      status = AFI_OK; 
    }
  }                                    /* end if file is ok */
  
 return( status );
}                      /* AFIsettoken */
