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
**************************************************************************
*
*    This file contains the structures for the file i/o package.
*    17 November 1988
*    E. Scott Daniels
*
**************************************************************************
*/

struct inputmgt_blk            /* input buffer management block for tokens */
 {
  struct inputmgt_blk *next;   /* these are a fifo queue of input buffers */
  char *idx;                   /* index at next char to process in buffer */
  char *end;                   /* index of 1 past last valid char in buf */
  int flags;                   /* flags- the IF_xxx constants */
  char *buf;
 };
 
struct tokenmgt_blk            /* token management block */
 {
  struct inputmgt_blk *ilist;  /* list of input buffers to process */
  char varsym;                 /* variable symbol */
  char escsym;                 /* escape symbol */
  char *tsep;                  /* list of chars that act as token seps */
  char *psep;                  /* parameter seperator character */
 };

struct pipe_blk                 /* info needed to manage a pipe */
 {
  int pipefds[2];               /* file descriptors  */
  char *cache;                  /* cache buffer */
  char *cptr;                   /* index into the current cache */
  char *cend;                   /* last piece of valid data in cache */
  int csize;                    /* number of bytes allocated for cache */
 };

struct file_block  /* defines an open file - pointed to by files array */
 {
  struct file_block *chain;     /* pointer to next file block in chain */
  struct pipe_blk *pptr;        /* pointer to pipe info if file is a pipe */
  struct tokenmgt_blk *tmb;     /* token management block for tokenized strm */
  char name[MAX_NAME+1];        /* name of the file */
  FILE *file;                   /* real file handle (pointer) */
  int flags;                    /* read write flags */
  int max;                      /* max number of characters to read/write */
  long operations;              /* number of operations on the file */
  Sym_tab *symtab;		/* variable lookup symbol table (supplied by user on settok call */
 };


 extern struct file_block **afi_files;

char *AFIisvar( char *tok, struct tokenmgt_blk *tmb );
