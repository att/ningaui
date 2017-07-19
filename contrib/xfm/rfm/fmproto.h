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
/* prototypes */


void FMaddtok( char *, int );
void FMasis( );
void FMbd( );
void FMbeglst( );
void FMbox( );
void FMbxend( );
void FMbxstart( int option, char *colour, int border, int width, char *align );
void FMccol( int );
void FMcd( );
void FMceject( );
void FMcenter( );
void FMclose( );
int FMcmd( char *);
void FMcpage( );
void FMcpage( );
void FMdefheader( );
void FMditem( );
void FMdv( );
void FMelse( );
void FMendlist( int );
void FMep( );
int FMevalex( char *  );
void FMfigure( );
void FMflush( );
void FMformat( );
int FMgetparm( char **);
int FMgetpts( char *, int );
int FMgettok( char ** );
void FMgetval( );
/*struct var_blk *FMgetvar( char *); */
char *FMgetvar( char *, int ); 
void FMheader( struct header_blk *);
void FMhn( );
void FMif( );
void FMimbed( );
void FMindent( int * );
int FMinit( int, char ** );
void FMjustify( );
void FMline( );
void FMlisti( );
void FMll( );
void FMmsg( int, char *);
void FMnofmt( );
int FMopen( char *);
void FMpara( int, int );
void FMpflush( );
void FMpgnum( );
int FMread( char *);
void FMright( );
void FMrunout( int, int);
void FMrunset( );
void FMsection( );
void FMsetcol( int );
void FMsetfont( char *, int );
void FMsetjust( );
void FMsetmar( );
void FMsetstr( char **, int );
void FMsetx( );
void FMsety( );
void FMspace( );
void FMtc( );
void FMtmpy( int );
void FMtoc( int );
int FMtoksize( char *, int );
void FMtopmar( );
int FMvartok( char ** );
