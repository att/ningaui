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
 --------------------------------------------------------------------------
   Mnemonic: basics.h
   Abstract: Some basic things needed by most programmes:
               ARG_START, ARG_END argument processing
             -x[value] flags may be entered as: -x value or -xvalue. Flags
             that do not have any operand (value) may be entered as -xyz or 
             -x -y -z.
             LARG, IARG, FARG, and SARG MUST be used if flag has an operand.
             FLARG (flag set) and INC_ARG (increment) may optionally be used.
   Date:     2 Feb 2000
   Author:   E. Scott Daniels
   Modified:
 --------------------------------------------------------------------------
*/

 char *argv0;   /* program name */

/* NOTE: argument processing destroys argv and argc, if needed after arg 
         processing then the user should save BEFORE any of these macros are
         utalised!
*/
#define LARG(_x) (_x)=(*(tok+2))?strtol(tok+2,0,0):(argc>0?strtol(*(argv++),0,0):0),argc--;tok=0
#define IARG(_x) (_x)=(int) (*(tok+2))?strtol(tok+2,0,0):(argc>0?strtol(*(argv++),0,0):0),argc--;tok=0
#define FARG(_x) (_x)=(*(tok+2))?atof(tok+2):(argc>0?atof(*(argv++)):0),argc--;tok=0
#define SARG(_x) (_x)=(*(tok+2))?(tok+2):(argc>0?(*(argv++)):0),argc--;tok=0
#define FLARG(_x,_y) (_x)|=(_y)
#define INC_ARG(_x) (_x)++

#define ARG_START { char *tok=0; argv0=*(argv++); argc--; while( tok || (argc > 0 && **argv == '-') ) { if( ! tok || ! *tok ){ tok=*(argv)++; argc--;} switch(*(tok+1)) {

#define ARG_END default: fprintf( stderr, "%s: unknown flag: %s\n", argv0, tok ); usage( ); exit( 1 ); break; } if( tok ) if( ! *((++tok)+1) ) tok=0;} }
