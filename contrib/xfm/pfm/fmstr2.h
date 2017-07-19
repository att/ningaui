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
*   Abstract: This file contains the structure definitions for the
*             formatter utility.
*   Date:     16 November 1988
*   Author:   E. Scott Daniels
*   Modified: 30 Apr 1989 - To support list item characters
*             22 Apr 1991 - To add head/foot string len vars
*                           To remove need for page number buffer.
*             23 Apr 1991 - To reduce page length default by 2 for footers
*                           To add toc file name buffer.
*                           To add header left margin hlmar.
*             3  May 1992 - To support post script output
*             13 Jun 1993 - To remove unneeded global vars (set struct for
*                           list)
**************************************************************************
*/

 struct col_blk
  {                        /* column management block */
   struct col_blk *next;   /* pointer at next block in the list */
   int width;              /* width (points) of the column */
   int lmar;               /* base left margin of this column */
  };

 struct fblk
  {
   struct fblk *next;     /* pointer to the next block in the list */
   int file;              /* file number of the file */
   char name[80];         /* name of the file */
   int status;            /* current file status */
   long count;            /* number of records processed for the file */
   int fnum;              /* internal imbed number of the file */
  };

 struct li_blk
  {                       /* list item character block */
   struct li_blk *next;   /* next block in the list */
   unsigned char ch;      /* character to be placed on the list item */
   int xpos;              /* x position for item mark character */
   char *font;            /* pointer at font name */
   int size;              /* font size */
   int yindex;            /* index into y position array */
   int ypos[60];          /* positions of item mark in y direction */
  };

 struct box_blk
  {                       /* box information */
   int topy;              /* top y value of the box */
   int lmar;              /* left margin of the box */
   int rmar;              /* right margin of the box */
   int hconst;            /* horizontal line opt - if true then hz line on */
   int vcol[MAX_VCOL];    /* vert col locations (points, rel to lmar) */
  };

 struct var_blk
  {                                  /* user defined variable */
   struct var_blk *next;             /* pointer at the next one */
   char name[9];                     /* variable name */
   char string[MAX_VAREXP+1];        /* expansion string */
  };

 struct header_blk               /* block to hold header related variables */
  {
   char *font;                   /* font for this header */
   int size;                     /* point size for the header */
   int flags;                    /* header flags */
   int indent;                   /* number of spaces to indent the 1st line */
   int skip;                     /* space to skip before header (ptr) */
   int level;                    /* paragraph level */
   int hmoffset;                 /* amount to space in from header margin */
  };

 extern struct box_blk box;        /* the box structure */
 extern struct li_blk *lilist;     /* pointer at the list item list */
 extern struct fblk *fptr;         /* pointer to the file block list */
 extern struct var_blk *varlist;   /* pointer at the list of variables */

 extern int tocfile;               /* table of contents file pointer */
 extern char tocname[15];          /* buffer to put toc file name in */

 extern int flags;                     /* processing flags */
 extern int flags2;                    /* second set of processing flags */
 extern int tokloc;                    /* location flags of token */

 extern long words;         /* number of words in the document */
 extern int fig;            /* figure number - always @ next one */
 extern int lmar;           /* initial leftmargin value (points) */
 extern int hlmar;          /* header left margin (points) */
 extern int pageshift;      /* page shift for two side */
 extern int osize;          /* size of info in output buffer (points) */
 extern int linelen;        /* line length (points) */
 extern int linesize;       /* size of a line (pts) drawn by .ln */
 extern char vardelim;      /* variable delimiter */

 extern int cury;
 extern int textsize;
 extern int textspace;
 extern char runfont[50];       /* running head/foot font */
 extern char *curfont;          /* current text font */
 extern char *difont;           /* definition list defintion font */
 extern char *ffont;            /* figure font */
 extern char *ocache;
 extern int runsize;
 extern int boty;
 extern int topy;
 extern int rtopy;              /* real topy value if temp top set */
 extern int rtopcount;         /* real topy reset column count */
 extern int dlstack[10];               /* definition list margin stack */
 extern int dlstackp;
 extern int fillgrey;           /* fill grey shade */

 extern char *rhead;
 extern char *rfoot;

 extern char obuf[MAX_READ];           /* output buffer */
 extern int optr;
 extern char inbuf[MAX_READ];          /* input buffer */
 extern char *varbuf;
 extern int iptr;
 extern int viptr;
 extern int ofile;                     /* file number of output file */

 extern int cenx1;                     /* centering points */
 extern int cenx2;

 extern int page;

 extern struct header_blk *headers[MAX_HLEVELS];

 extern int pnum[ ];

 extern struct col_blk *firstcol;      /* pointer at column list */
 extern struct col_blk *cur_col;

 extern char *messages[ ];
