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
/* DEPRECATED -- include ut.h --- */







#ifndef __UTPROTO_
#define __UTPROTO

char *UTgetevar( char **elist, char *str );
void UTtimes( int *m, int *d, int *y );
void UTrotate( double x, double y, double *rx, double *ry, double cx, double cy, int angle );


/* -----------  utmalloc.c -------------------------- */
void *UTmalloc( int i );
void *UTrealloc( void *, int i );
char *UTstrdup( char *str );

/* -------------- time.c --------------------------- */
char *cvt_odate( char *ostr );			/* convert oracle style time into string */
unsigned long UTtime_ss2ts( char *d, char *t );    /* convert date string (d) and time string (s) into integer date is yyyy/mm/dd or mm/dd/yyyy */
unsigned long UTtime_s2ts( char *s );		/* convert a sigle string containing date and time into integer */
unsigned long UTtime_now();			/* current time */
int *UTtime_ts2i( unsigned long ts );  		/* convert timestamp into an array integers (m,d,y,h,min,s) */
char *UTtime_ts2dow( unsigned long ts );	/* convert timestamp into day of week string */
char *UTtime_ts2s( unsigned long ts );		/* convert timestamp into string */


#endif
