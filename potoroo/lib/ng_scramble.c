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

/*
# Mnemonic:	ng_scramble
# Abstract:	simple buffer scrambling/descrambling
#		
# Date:		22 Apr 2008 (imoekdjr)
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
*/

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <ningaui.h> 

/* this is just a simple buffer scramble algorithm; not meant to be secure, but to keep 
   things from being completely in the clear if deemed necessary.
   The buffer returned contains the key start index and then the scrabmbled buffer. 
   the length is the original buffer length +1, and the returned buffer is NOT a string.
   The values in both key and buffer are expected to be <0x80 and we enforce this.
   We also go to some length to ensure that data in the scrambled buffer is in the range of
   0x20 to 0xfe; this is to allow the buffer to be passed into an application that is using 
   newlines or some low character as a message seperator.

   For sending purposes, the length of the scrambled buffer is two bytes longer than the 
   original buffer. 
*/
static int initialised = 0;

unsigned char *ng_scramble( unsigned char *key,  unsigned char *buf )
{
	static unsigned char pidx = 19;
	int klen;
	unsigned char *sp;
	int i;
	int o = 0x20;
	unsigned char *sbuf;

	if( ! initialised++ )
		srand( time( NULL ) );
		

	klen = strlen( key );

	sbuf = (unsigned char *) ng_malloc( sizeof( unsigned char ) * (strlen( buf ) + 2 ), "scr" );
	*sbuf = (rand() % klen) %256 + 0x20;
	i = ((unsigned int) *sbuf) - 0x20;
	
	for( sp = sbuf+1; *buf; buf++, sp++ )
	{
		*sp = (*buf ^ (key[i] & 0x7f)) + o;
		if( ++i >= klen )
			i = 0;
		if( ++o >= 127 )
			o = 0x20;
	}

	*sp = 0xff;
	
	return sbuf;
}

/* unscramble a buffer scrambled by the previous function 
   we assume that the original buffer scrambled was a string, we tack a trailing null 
   onto the converted buffer.
*/
unsigned char *ng_unscramble( unsigned char *key,  unsigned char *sbuf )
{
	int klen;
	unsigned char *bp;
	int i;
	int o = 0x20;
	unsigned char *buf;

	klen = strlen( key );

	i = 0;
	for( bp = sbuf; *bp != 0xff; bp++, i++ );
	buf = (unsigned char *) ng_malloc( sizeof( unsigned char ) * i, "unscr" );
	i = ((unsigned int) *sbuf) - 0x20 ;
	
	sbuf++;				/* skip start value */
	for( bp = buf; *sbuf != 0xff; bp++, sbuf++ )
	{
		*bp = (*sbuf - o) ^ key[i];
		if( ++i >= klen )
			i = 0;
		if( ++o >= 127 )
			o = 0x20;
	}

	*bp = 0;
	
	return buf;
}



/* like scramble, but it does not place range limits on the input data or key
   and scrambled data may be 00-0xff.  The resulting buffer is the one byte longer
   than the original input buffer.  The extra byte is used to hold the start location
   in the key.
*/
unsigned char *ng_scramble256( unsigned char *key, int klen,  unsigned char *buf, int blen )
{
	static unsigned char pidx = 19;
	unsigned char *sp;
	int i = 0;
	unsigned char *bp;
	unsigned char *sbuf;

	if( ! initialised++ )
		srand( time( NULL ) );

	sbuf = (unsigned char *) ng_malloc( sizeof( unsigned char ) * (blen+1), "scr256" );
	*sbuf = (rand() % klen) %256;
	i = ((unsigned int) *sbuf);
	
	bp = buf;
	for( sp = sbuf+1; bp < buf+blen; bp++, sp++ )
	{
		*sp = (*bp ^ key[i]) + i;
		if( ++i >= klen )
			i = 0;
	}

	return sbuf;
}

/* unscrambles a buffer scrambled using the 256 function above. length of buffer is one less
   than sblen passed in 
*/
unsigned char *ng_unscramble256( unsigned char *key, int klen,  unsigned char *sbuf, int sblen )
{
	static unsigned char pidx = 19;
	unsigned char *sp;
	unsigned char *bp;
	unsigned int i = 0;
	int si = 0;
	unsigned char *buf;


	bp = buf = (unsigned char *) ng_malloc( sizeof( unsigned char ) * (sblen-1), "unscr256" );
	i = ((unsigned int) *sbuf);
	
	for( sp = sbuf+1; sp < sbuf+sblen; bp++, sp++ )
	{
		*bp = (*sp - i) ^ key[i];
		if( ++i >= klen )
			i = 0;
	}

	return buf;
}



#ifdef SELF_TEST
int main( int argc, char **argv )
{
	int i;
	int j;
	unsigned int hv;
	unsigned char	*b1 = NULL;
	unsigned char	*b2 = NULL;
	unsigned char	*b3 = NULL;
	char obuf[2048];

	b1 = strdup( "now is the time for all good men to come to the aid of their country !@#$%^&*()_-+=`~" );

	for( j = 0; j < 10; j++ )
	{
		b2 = ng_scramble( argv[1], b1 );

		hv = 0;
		printf( "%02x ", b2[0] );
		for( i = 1; b2[i] != 0xff; i++ )
		{
			if( b2[i] < 0x20 )
				printf( "   >>>" );
			if( b2[i] > hv )
				hv = b2[i];
			printf( "%c", isprint(b2[i]) ? b2[i] : '?' );
		}
		ng_free( b2 );
			/*printf( "%02x ", b2[i] ); */
		printf( "\t hv=%x\n", hv  );
		sleep( 1 );					/* sleep to force a different key starting point */
	}

	b3 = ng_unscramble( argv[1], b2 );
	printf( " %s\n", b3 );
	ng_free( b3 );

	/* test 256 scramble */
	hv = strlen(b1);
	for( i = 0; i < hv;  i++ )
		printf( "%02x ", b1[i] );
	printf( "\n\n" );
	for( j = 0; j < 10; j++ )
	{
		b2 = ng_scramble256( argv[1], strlen( argv[1] ), b1, strlen(b1) );

		for( i = 0; i < hv+1;  i++ )
			printf( "%02x ", b2[i] );
		printf( "\n"  );
		ng_free( b2 );
		sleep( 1 );
	}

	b3 = ng_unscramble256( argv[1], strlen(argv[1]),  b2, hv + 1 );
	printf( "\n" );
	for( i = 0; i < hv;  i++ )
		printf( "%02x ", b3[i] );
	printf( "\n" );
	ng_free( b2 );
}
#endif

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_scramble:Simple buffer scrambling/unscrambling)

&space
&synop	ng_scramble( unsigned char *key, unsigned char *buf )
&break	ng_unscramble( unsigned char *key, unsigned char *buf )
&break  ng_scramble256( unsigned char *key, int keylen, unsigned char *buf, int buflen )
&break  ng_unscramble256( unsigned char *key, int keylen, unsigned char *sbuf, int sbuflen )

&space
&desc
	These routines provide a simple means to scramble the contents of an ascii string.
	The scrambling is NOT meant to be secure, but can serve to keep things from 
	being stored/tramsmitted in the clear if needed


&space
	The &lit(ng_scramble) funciton accepts a key and buffer and returns a pointer
	to malloced data that has been scrambled.  The length of the returned data is
	two bytes larger than the size of the buffer containing the clear text; the returned data is NOT a string so 
	the data cannot be the target of str*() functions.

&space
	The source buffer and key contents are expected to be character values less than 0x80. 
	The scrambled buffer is guarenteed not to have values less than 0x20 which allows the 
	buffer to be read/written using a newline, or other low character, as a record seperartor. 

&space
	The &lit(ng_unscramble) accepts a key, and a scrambled buffer. A pointer to 
	a newly allocated buffer is returned.  

&space
	The user is responsible for releasing the storage allocated by either of these
	functions.

&space

&subcat The 256 functions
	The &lit(ng_scrabmle256) and &lit(ng_unscramble256) functions are similar to the other 
	scramble functions except they place no limitations on the contents of either the key 
	nor the buffer, and each value in the scrambled buffer can be in the range of 00 through
	0xff.  The length of the scrambled data returned by &lit(ng_scramble256) is one byte 
	larger than the original source buffer. The length of the data returned by *lit(ng_unscramble256)
	will be one byte less than the data passed in. 
&exit

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	22 Apr 2008 (sd) : Its beginning of time. 
&term 	25 Apr 2008 (sd) : Added the 256 functions.
&endterms


&scd_end
------------------------------------------------------------------ */

