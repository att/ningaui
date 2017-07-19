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
*   Abstract: This file contains the constants for the formatter package.
*   Date:     15 November 1988
*   Author:   E. Scott Daniels
*
*   Modified: 23 Apr 1991 - To remove toc file name const.
*              3 May 1992 - To convert to post script output
*              1 Oct 1992 - To support placing extra space after punctuation
*             21 Dec 1992 - Physically removed command const definitions.
*             12 Jul 1993 - Constants to support right justified def items
**************************************************************************
*/
/* used TRACE( 1 ) "format string", parm, parm, parm ); */
#define TRACE(_a) if( trace >= (_a)) fprintf( stderr, 

#define EOS  '\000'            /* end of string character */
#define BLANK      ' '         /* blank character */
#define VALID      0           /* processing results were valid */
#define OK         0           /* processing was ok */
#define ERROR     -1           /* error occured */
#define TRUE       1           /* true and false constants */
#define FALSE      0

#define DI_ANUMI      1         /* integer di auto numbering */
#define DI_ANUMA      2         /* alpha di auto numbering */

#define MAX_READ      2048     /* size of read, max size of column */
#define MAX_FILES     10       /* max level of imbed files permitted */
#define MAX_HLEVELS   4        /* max number of header levels */
#define MAX_VCOL      10       /* maximum number of vertical columns */
#define MAX_VARNAME   31       /* maximum number of chars in variable name */
#define MAX_VAREXP    2048     /* max number of chars a var can expand to */
#define HEADFOOT_SIZE 80       /* number of characters in the header/footer */
#define MAX_Y         690      /* max number of points in the y direction */
#define MAX_X         580      /* max number of points in the x direction */
#define MAX_DEFLIST   9        /* max number of concurrent def lists */
#define MAX_LIITEMS   60       /* max number of list items per page */
#define MAX_COLS      4        /* maximum columns of text on the page */
#define MAX_FONT      50       /* max length of the font string */
#define MAX_LL        200      /* max characters per line */
#define MAX_CELLS     100      /* max cells in a table row */

#define DEF_TOPY      45       /* default top y value for text */
#define DEF_BOTY      550      /* default bottom y value for text */
/*#define DEF_BOTY      690 */     /* default bottom y value for text */
#define DEF_TEXTSIZE  10       /* default text size (points) */
#define DEF_LINESIZE  3        /* default line size (pts) for .ln cmd */
#define DEF_LINELEN   450      /* default line length points */
#define DEF_SPACE     3        /* number of points between lines */
#define DEF_LMAR      54       /* default number of points lmar is */
#define DEF_GUTTER    15       /* default gutter space between columns */
#define DEF_TEXTFONT  "Times-Roman"  /* default font names */
#define DEF_RUNFONT   "Helvetica"  /* default running head/foot font */
#define DEF_HEADFONT  "Helvetica"   /* default header font */
#define DEF_LIFONT    "ZapfDingbats"   /* defautl font for list items */
#define DEF_HEADINDENT 5       /* default indention for headers */
#define DEF_RUNSIZE    10      /* default running size */
#define DEF_H1SIZE     18
#define DEF_H2SIZE     14
#define DEF_H3SIZE     12      /* default header sizes */
#define DEF_H4SIZE     12
#define DEF_LICHR      '*'     /* default list item character */
#define LIST_CHR       '*'     /* symbol for list items */

#define PTWIDTH       7        /* width of standard character in points */
#define PG_WIDTH_PTS  560      /* width of a page in points */

                               /* flag constants (max value 0x80) */
#define NOFORMAT      0x01     /* do not format input lines */
#define DOUBLESPACE   0x02     /* double space flag */
#define TWOCOL        0x04     /* indicates two column mode */
#define PARA_NUM      0x08     /* indicates we are numbering paragraphs */
#define PAGE_NUM      0x10     /* numbering pages when on */
#define JUSTIFY       0x20     /* justify text in output file */
#define TOC           0x40     /* table of contents enabled */
#define LIST          0x80     /* indicates list in progress (last one) */


                               /* flags2 constants (max 0x8000) */
#define BOX           0x01     /* box is in progress */
#define F2_BOX        0x01     /* box is in progress */
#define F2_2SIDE      0x02     /* in twosided mode */
#define F2_SETFONT    0x04     /* flush must set font before flushing */
#define F2_PUNSPACE   0x08     /* space after token ending in punctuation */
#define F2_CENTER     0x10     /* flush should cen rather than show */
#define F2_RIGHT      0x20     /* flush should right rather than show */
#define F2_TRUECHAR   0x40     /* flush will true charpath rather than show */
#define F2_ASIS       0x80     /* put input into output (assume postscript) */
#define F2_SAMEY      0x100    /* dont bump up cur y on flush */
#define F2_DIRIGHT    0x200    /* right justify def list items */
#define F2_NOISY      0x400    /* when set display all info type messages */
#define F2_BORDERS    0x800    /* table should have borders */
#define F2_QUOTED     0x1000   /* inside of back quote marks */
#define F2_DIBUFFER   0x2000   /* output contains a def list term */
#define F2_OK2JUST    0x4000   /* ok to justify - set by add tok in tfm */
#define F2_SMASH      0x8000	/* smash next token to previous one */

#define F3_HYPHEN	0x01 	/* hyphinate words if too long (psfm) */
#define F3_NOFONT	0x02	/* no <font> things in hfm if doing css work */
#define F3_IDX_SNARF	0x04	/* pass words to doc-index snarfing */

#define LF_INCHES     0x01     /* routine local flags - inches entered */
#define LF_POINTS     0x02     /* points entered */
#define LF_CLOSE      0x04     /* close y space */
#define LF_CENTER     0x08     /* center artwork */
#define LF_NOADV      0x10     /* dont advance current y */
#define LF_PROPORT    0x20     /* keep ep proportional in x and y direction */
#define LF_STRETCH    0x40     /* stretch ep to fit space */

#define	CF_SKIP		0x01	/* column block flags - skip this when drawing vert lines in table */

#define ADD           1        /* add/sub parameter from current setting */

#define START         1        /* token was located at start of line */
#define MIDDLE        2        /* token was located somewhere in the middle */
#define FIRST         3        /* row is in the middle of the table */

#define CMDSYM       '.'       /* command indication symbol in first col */
#define EX_SYM       '['       /* symbol that "opens" an expression */
#define CONTINUE_SYM '~'       /* parameter continuation symbol */

                               /* flag constants for the hflag field */
#define HTOUPPER      0x01     /* header should be all caps */
#define HEJECT        0x02     /* header should start new page */
#define HEJECTP       0x02     /* header to start on new page */
#define HTOC          0x08     /* header should be listed in table of conte */
#define HBOLD         0x10     /* header should be in bold type */
#define HEJECTC       0x20     /* header to start at top of new column */

                               /* message constants */
#define I_EOFREACHED  1        /* end of file reached message */
#define E_MAXFILES    2        /* too many files imbeded */
#define E_CANTOPEN    3        /* unable to open file */
#define E_MISSINGNAME 4        /* name on command statement missing */
#define I_IMBED       5        /* imbedding a file into document */
#define E_NOMEM       6        /* unable to get storage */
#define I_LINES       7        /* number of lines imbedded = */
#define E_MISSINGPARM 8        /* parameter missing from command line */
#define I_UNKNOWN     9        /* unknown command entered */
#define I_HELLO       10       /* fm started message */
#define I_WORDCNT     11       /* number of words placed in the document */
#define E_TAKEOUT     12       /* parameter takes margin out of range */
#define E_TERM_TOOBIG 13       /* termsize larger than line len */
#define E_LMAR_TOOLIT 14       /* left margin smaller than term size */
#define W_LL_TOOBIG   15       /* line length entered is too large */
#define E_UNEXEOF     16       /* unexpected end of file reached */
#define E_PARMOOR     17       /* parameter out of range */
#define E_BBOX        18       /* bounding box info is missing or bad */
#define E_NO_DEFLIST  19       /* definition list not started */
#define E_UNKNOWNPARM 20       /* unrecognized parameter on command */
