/*
* ======================================================================== v1.0/0a157 
*                                                         
*       This software is part of the AT&T Ningaui distribution 
*	
*       Copyright (c) 2001-2009 by AT&T Intellectual Property. All rights reserved
*	AT&T and the AT&T logo are trademarks of AT&T Intellectual Property.
*	
*       Ningaui software is licensed under the Common Public
*       License, Version 1.0  by AT&T Intellectual Property.
*                                                                      
*       A copy of the License is available at    
*       http://www.opensource.org/licenses/cpl1.0.txt             
*                                                                      
*       Information and Software Systems Research               
*       AT&T Labs 
*       Florham Park, NJ                            
*	
*	Send questions, comments via email to ningaui_support@research.att.com.
*	
*                                                                      
* ======================================================================== 0a229
*/

#include <stdio.h>
#include <ctype.h>
#include 	<stdlib.h>
#include	<string.h>

#include <sfio.h>

#include <ningaui.h>

/*
--------------------------------------------------------------------------------
  Mnemonic: dump
  Abstract: This function will display n bytes of storage for in hex dump
            format and will provide a cheater block to the right side of the 
            display.  The display is written to standard error.  
            If the flags has the D_OFFSET bit set (not really) then the 
            offset of each line from the start of the buffer is displayed to 
	    the left of the data.  If the D_ADDR flag is set, then the actual
	    address is displayed with each line.  If the D_EBCDIC bit is set
            in the flags, then the data is assumed to be in EBCDIC and the 
            cheater block is formatted with that assumption. If more than 
            one line of output is equal to the last line it is removed and 
	    the string SNIP is placed in the output area. Display resumes with 
	    the next line that contains different data, or the last line of
	    the display.
  Parms:    f - File pointer  
            p - Pointer to beginning of storage to display
            l - Number of bytes to display
            f - Flags (D_OFFSET | D_EBCDIC | D_ADDR | D_ROFF | D_DECIMAL | D_ALL)
            s - Pointer to a string to put at the start of the display
  Returns:  Nothing.
  Date:     Sometime in September 1997
  Author:   E. Scott Daniels
--------------------------------------------------------------------------------
*/

/* EBCDIC character set */
char *cs_ebcdic =   "................................\
................................\
 ...........<(+|&.........!$*);.\
-/.........,%->?..........:#@'=\"\
.abcdefghi{......jklmnopqr}.....\
..stuvwxyz......................\
{ABCDEFGHI......}JKLMNOPQR......\
..STUVWXYZ......0123456789......";


void atoe( unsigned char *s )   /* translate ascii string  to ebcdic */
{
	for( ; *s; s++ )
		*s = cs_ebcdic[*s];
}

void fdump( Sfio_t *f, unsigned char *data, int len, int flags, char *s )
{
	static long roffset = 0;    /* running offset */
	int i;                      /* loop index */
	int j = 0;                  /* loop index */
	unsigned char *end;         /* pointer at end of dump area */
	unsigned char cheater[20];  /* space to build cheater characters in */
	int nrow;
	
	if( ! data )
	{
	sfprintf( sfstderr, "dump: null pointer passed in.\n" );
	return;
	}
	
	end = data + len;
	
	if( ! (flags & D_ROFF) )
	sfprintf( f, "%s %p for %d bytes %s\n", s ? s : "DUMP:", data, len, flags & 0x01 ? "EBCDIC" : "" );

	nrow = (flags & D_ADDR)? 20 : 16;
 	while( data < end  )
  	{
		if( flags & D_DECIMAL )
		{
			if( flags & D_ADDR )                      /* show addr on left */
				sfprintf( f,  "%08d: ", data );
			else                                      /* show offset on left */
				sfprintf( f,  "%06d: ", j + roffset );
		}
		else
		{
			if( flags & D_ADDR )                      /* show addr on left */
				sfprintf( f,  "%08X: ", data );
			else                                      /* show offset on left */
				sfprintf( f,  "%06X: ", j + roffset );
		}
	
		for( i = 0; data < end && i < nrow; i++ )
		{
			sfprintf( f, "%02X ", *data );
			cheater[i] = 
			flags & D_EBCDIC ? (*data ? *data : 1) : isprint( *data ) ? *data : '.';
			data++; 
    		} 
	
		cheater[i] = 0;
		
		j += i;

		for( ; i < nrow; i++ )
			sfprintf( f, "   " );        /* line up last cheater */
	
		if( flags & D_EBCDIC )
			atoe( cheater );
	
		sfprintf( f, "  %s\n", cheater );

		if( !(flags & D_ALL)  &&  data + nrow < end &&  memcmp( data - nrow, data, nrow ) == 0 )
		{
			sfprintf( f, "      ---------------- SNIP --------------\n" );
			j += nrow;
			for( data += nrow; data + nrow < end &&  memcmp( data, data - nrow, nrow ) == 0; data += nrow )
				j += nrow; 
		}
 	}

 	if( ! (flags & D_ROFF) )
 		sfprintf( f, "\n" );
	else
	 	roffset += j;
} 

/* allow old programs to continue to work with new capability */

void dumps( unsigned char *data, int len, int flags, char *s )
{
 fdump( sfstderr, data, len, flags, s );
}

void dump( unsigned char *data, int len, int flags )
{
 fdump( sfstderr, data, len, flags, NULL );
}

#ifdef KEEP
--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&doc_title(fdump:Dump an area of memory to file)

&space
&synop	
	#include <fcntl.h>
&break fdump( Sfio_t *f, unsigned char *buf, int len, int flags, char *msg );

&space
&desc	The &ital(fdump()) routine will dump in hexadecimal format with 
	a cheater block the bytes that are pointed to by buf. The 
	string pointed to by message is written above the output of the 
	routine. 

&space
&parms	The following are the parameters that must be passed to this routine.
&begterms
&term 	f : The pointer to the &lit(Sfio_t) structure of the file to write the 
	output to.
&term 	buf : The pointer to the area in memory to begin the write at. 
&term   len : The number of bytes to be examined and formatted. 
&term   flags : 1 if the data is assumed to be EBCDIC, the cheater block 
	will be formatted assuming an EBCDIC character set. ASCII is assumed
	if 0.  
&term   msg : Pointer to a string that is written above the dump and can be 
	used to identify the data. 
&endterms

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	1 Sep 1997 (sd) : Original code.
&term	20 Apr 2001 (sd) : Converted from Gecko.
&term	31 Jul 2005 (sd) : changes to prevent gcc warnings.
&endterms
&scd_end
#endif
