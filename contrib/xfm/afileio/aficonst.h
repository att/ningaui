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
*  Mnemonic: aficonst.h
*  Abstract: This file contains constants that are for internal use of the 
*            AFI routines only. This file should NOT be included by any
*            user based subroutines.
*  Author:   E. Scott Daniels
*
***************************************************************************
*/

#define EOS     '\000'     /* end of string marker */
#define TRUE    1
#define FALSE   0

#define AFIHANDLE int   /* file handle "type" */

                        /* indexes into file descriptor array for a pipe */
#define READ_PIPE  0    /* fd for the read end of the pipe */
#define WRITE_PIPE 1    /* fd for the write end of the pipe */

#define FIRST   00
#define NEXT    01

#define F_AUTOCR  0x01   /* flags - auto generate a newline on write */
#define F_BINARY  0x02   /* binary data - dont stop read on newline */
#define F_STDOUT  0x04   /* output to standard output */
#define F_NOBUF   0x08   /* dont buffer data on write - immed flush */
#define F_STDIN   0x10   /* read from stdinput */
#define F_EOF     0x20   /* end reached on input - really need for stdin only*/
#define F_PIPE    0x40   /* "file" is a pipe */
#define F_WHITE   0x80   /* return white space as a token */
#define F_EOBSIG  0x100  /* set end of buffer signals on real reads */

                         /* input buffer management block flags */
#define IF_EOBSIG 0x01   /* signal end of buffer to user, dont auto stream */

#define DOT     '.'      /* dot character returned by directory look */

#define MAX_FILES 20     /* maximum number of file chains that can be open */
#define MAX_NAME  1024     /* number of characters in the file name */
#define MAX_CACHE 2048   /* size of the cache for pipes */
#define MAX_TBUF  2048   /* max size of a token management input buffer */


#define TOKENMGT_BLK 1   /* block types to allocate in new */
#define INPUTMGT_BLK 2
