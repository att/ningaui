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
****************************************************************************
*
*  Mnemonic: FMcmds.h
*  Abstract: This file contains the constants that the FMcmd routine uses
*            in its switch statements. Each constant represents a valid
*            PFM dot (.) command.
*
****************************************************************************
*/
#define C_ABORT       0x4142   /* .ab abort - exit w/o cleanup */
#define C_ASIS        0x4149   /* .ai as is command */
#define C_BLOCKCTR    0x4243   /* .bc block center start/end */
#define C_BEGDEFLST   0x4244   /* .bd begin definition list */
#define C_BEGLIST     0x424C   /* .bl begin list */
#define C_BREAK       0x4252   /* .br break line */
#define C_BOX         0x4258   /* .bx box start or end command */
#define C_BOTY        0x4259   /* .by set bottom y pts value */
#define C_CAPTURE     0x4341   /* .ca capture lines to a file */
#define C_CDEFINE     0x4344   /* .cd column define command */
#define C_CEJECT      0x4342   /* .cb colum begin (eject) */
#define C_CENTER      0x4345   /* .ce center */
#define C_CCOL        0x4343   /* .cc conditional column eject */
#define C_TABLECELL   0x434C   /* .cl start next table cell */
#define C_COLOR       0x434F   /* .co set rgb color for text */
#define C_COLOUR      0x434F   /* .co set rgb color for text */
#define C_COMMENT     0x2A2A   /* .** is a comment line; input line skipped */
#define C_CPAGE       0x4350   /* .cp conditional page eject */
#define C_CSS	      0x4353	/* .cs cascading style sheet division info - hfm only */
#define C_COMMA		0x4354	/* .ct comma-tise the next token */
#define C_DEFDELIM    0x4444   /* .dd define variable delimiter (default &) */
#define C_DEFHEADER   0x4448   /* .dh define header command */
#define C_DEFITEM     0x4449   /* .di definition list item */
#define C_DOLOOP      0x444F   /* .do do loop */
#define C_DOUBLESPACE 0x4453   /* .ds double space mode */
#define C_DEFVAR      0x4456   /* .dv define variable */
#define C_ENDDEFLST   0x4544   /* .ed end definition list */
#define C_ELSE        0x4549   /* .ei else portion of if statement */
#define C_ENDLIST     0x454C   /* .el end list */
#define C_EP          0x4550   /* .ep embedded PostScript */
#define C_ENDTABLE    0x4554   /* .et end table command */
#define C_EVAL        0x4556   /* .ev evaluate - calc [ ] and include ans */
#define C_FIGURE      0x4647   /* .fg figure */
#define C_FORMAT      0x464F   /* .fo format command */
#define C_ENDIF       0x4649   /* .fi end if */
#define C_GREY        0x4752   /* .gr set grey scale 0=white 10=black */
#define C_GETVALUE    0x4756   /* .gv get an FM value into a variable */
#define C_H1          0x4831   /* .h1 header level 1 */
#define C_H2          0x4832   /* .h2 header level 2 */
#define C_H3          0x4833   /* .h3 header level 3 */
#define C_H4          0x4834   /* .h4 header level 4 */
#define C_HDMARG      0x484D   /* .hm header margin */
#define C_HN          0x484E   /* .hn turn on/off header numbers */
#define C_HYPHEN      0x4859   /* .hy - turn on/off hyphination */
#define C_IF          0x4946   /* .if see if next lines should be included */
#define C_IMBED       0x494D   /* .im imbed a file command */
#define C_INDENT      0x494E   /* .in indent next line */
#define C_INDEX	      0x4958   /* .ix index oriented things */
#define C_JUMP        0x4A4D   /* .jm jump to token */
#define C_JUSTIFY     0x4A55   /* .ju justify mode toggle command */
#define C_LOWERCASE   0x4C43   /* .lc lower case to end of parm list */
#define C_LISTITEM    0x4C49   /* .li list item */
#define C_LL          0x4C4C   /* .ll line length command */
#define C_LINE        0x4c4E   /* .ln draw line */
#define C_SETSPACE    0x4C53   /* .ls line spacing (points) between lines*/
#define C_LINESIZE    0x4C57   /* .lw set line width (width of a horiz line) */
#define C_NOFORMAT    0x4E46   /* .nf no formatting */
#define C_NEWLINE     0x4e4c   /* .nl force new line in output file */
#define C_ONPAGEEJECT 0x4f45   /* .oe [all] <commands> to run on next page eject
/* .op reserved for open output file */
#define C_OUTLINE     0x4F55   /* .ou outline characters (true charpath) */
#define C_PAGE        0x5041   /* .pa page eject command */
#define C_PAGENUM     0x504E   /* .pn page number toggle */
#define C_PAGEMAR     0x504D   /* .pm page margin amount */
#define C_POP	      0x504F   /* .po pop state */
#define C_PUNSPACE    0x5053   /* .ps punctuation space */
#define C_PUSH	      0x5055   /* .pu push state */
#define C_QUIT        0x5155   /* .qu quit processing */
#define C_RFOOT       0x5246   /* .rf running footer */
#define C_RHEAD       0x5248   /* .rh running header */
#define C_RIGHT       0x5249   /* .ri right justify */
#define C_RSIZE       0x5253   /* .rs running h/f font size */
#define C_SECTION     0x5343   /* .sc generate rtf section break */
#define C_SETFONT     0x5346   /* .sf set font command */
#define C_SHOWV       0x5356   /* .sv show current variable contents */
#define C_SKIP        0x534B   /* .sk skip lines if not at top of col */
#define C_SMASH		0x534D /* .sm smash; no space before next token -- psfm need for (&lit(xxx))*/
#define C_SPACE       0x5350   /* .sp space command */
#define C_STOPRUN	0x5352 /* .sr special command put in by imbed to pop the run stack */
#define C_SINGLESPACE 0x5353   /* .ss turn on single space mode */
#define C_SETTXTSIZE  0x5354   /* .st set text size (points) */
#define C_SETX        0x5358   /* .sx set x position command */
#define C_SETY        0x5359   /* .sy set y position command */
#define C_TABLE       0x5441   /* .ta start table */
#define C_TABLEBRK    0x5442   /* .tb safe rfm table break */
#define C_TABLEHEADER	0x5448	/* .th string  -- command string pushed with each page break while in a table */
#define C_TABLEROW    0x5452   /* .tr start next table row */
#define C_TOC         0x5443   /* .tc turn on/off table of contents */
#define C_TMPFONT	0x5446	/* .tf temp font for tokens to ~ */
#define C_TOPMAR      0x544D   /* .tm top margin command */
#define C_TWOSIDE     0x5453   /* .ts two sided page layout */
#define C_TMPTOP      0x5454   /* .tt set topy to cury temporarly */
#define C_TOUPPER     0x5455   /* .tu n turn n characters to upper */
#define C_TRACE	      0x5858	/* .xx level  set trace level */
