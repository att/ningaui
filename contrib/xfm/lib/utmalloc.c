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
----------------------------------------------------------------------------------
	Mnemonic: 	UTmalloc, UTrealloc, UTstrdup
	Abstract: 	Wrappers that make code easier and provide aborts if 
			there are memory issues.
	Date:		28 May 2008 
	Author:		E. Scott Daniels
----------------------------------------------------------------------------------
*/
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void *UTmalloc( int size )
{
	void *p;

	if( (p = (void *) malloc( size )) == NULL )
	{
		fprintf( stderr, "abort: malloc error: %d bytes\n", size );
		exit( 3 );
	}

	memset( p, 0, size );
	return p;
}

void *UTrealloc( void *op, int size )
{
	void *p;

	if( op != NULL )
		return UTmalloc( size );

	if( (p = (void *) realloc( op, size )) != NULL )
		return p;

	fprintf( stderr, "abort: cannot realloc %d bytes\n", size );
	exit( 3 );
}

void UTfree( void *p )
{
	if( p )
		free( p );
}

char *UTstrdup( char *src )
{
	char *new;

	if( src == NULL )
		src = "";

	if( (new = strdup( src )) != NULL )
		return new;

	fprintf( stderr, "abort: cannot strdup for string: %s\n", src );
	exit( 3 );
}
