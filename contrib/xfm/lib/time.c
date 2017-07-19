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
----------------------------------------------------------------
 Mnemonic: time.c
 Abstract: This file contains several time oriented routines:
           cvt_odate - convert strange oracle date to normal date
           UTtime_ss2ts- Generate an integer time stamp from two strings
			one containing date one containing time.
           UTtime_s2ts -Generate an integer time stamp from a 
                       single date/time string

           Some might wonder why it is necessary to have a time 
           package when time, ctime and the like exist. Well, to 
           the best of my knowledge these are all fine and good
           except they will not generate a timestamp for any time 
           other than the current time. As we need timestamps for 
           times other than the current execution time, we must 
           do this. 

 Date:     16 May 2000
 Author:   E. Scott Daniels
 Note:     The leap year calculations in these functions are kept
           to a simple if( y/4 == 0 ) algorithm. This will proivde
           accurate leapyear calculation until the year 2100 which 
           is evenly divisable by 4, but not a leap year! Thus 
           those of us who are around to face Y2.1K will have to 
           change the algorithms a bit. Until then, we can party 
           like its 1999!
------------------------------------------------------------------
*/
#include <sys/types.h>
#include <unistd.h>    
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <float.h>


#include "ut.h"    

#define TIME_EPOCH 1997
#define SEC_DAY    86400
#define SEC_YEAR   31536000
#define SEC_HR     3600


/* days occuring before each month, need leap version for ts->string cvsn */
static int dbm[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
static int ldbm[] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 };
static char *soup = "janfebmaraprmayjunjulaugsepoctnovdec";

/* 
-------------------------------------------------------------------------------
  Mnemonic: cvt_odate
  Abstract: convert a bloody strange oracle date into something realistic
            oracle date: dd-mon-yyyy where mon is the three char month abbrev.
  Parms:    ostr - pointer to the oracle date string (dd-mon-year)
  Returns:  Character pointer to string mm/dd/yyyy
-------------------------------------------------------------------------------
*/
char *cvt_odate( char *ostr )
{
 char buf[100];            /* large, private buffer where we will work */
 char m[6];                /* work buffer for strstr call */
 int mn;                   /* month number jan == 1 */

 *m = tolower( *(ostr+3) );            /* snag the month abbreviation */
 *(m+1) = tolower( *(ostr+4) );
 *(m+2) = tolower( *(ostr+5) );
 *(m+3) = 0;
 mn = (((int)(strstr( soup, m ) - soup))/3) + 1;   /* convert abbrev to int */
 sprintf( buf, "%02d/", mn );                      /* initialise buffer */
 *(buf+3) = *ostr;                                 /* snag day */
 *(buf+4) = *(ostr+1);
 *(buf+5) = '/';                                   
 *(buf+6) = 0;                                     /* add eos for strcat */
 strcat( buf+6, ostr+7 );                          /* slam on the year */
 *(buf+10) = 0;
 
 return strdup( buf );       /* new string; caller must free */
}

/*
-------------------------------------------------------------------------------
  Mnemonic: UTtime_ss2ts
  Abstract: Convert a date string and a time string into an integer
            timestamp. The time stamp is the number of seconds 
            past january 1 1997.  The epoch of 1997 is used as 
            it makes leap year calculations for years between 
            the epoch and the date passed in very simple.
  Params:   d - pointer to date string in either yyyy/mm/dd or 
                mm/dd/yyyy format. 4 digit years assumed.
            t - Pointer to time string in the format hh:mm[:ss].
  Returns:  Unsigned long integer timestamp.
-------------------------------------------------------------------------------
*/
unsigned long UTtime_ss2ts( char *d, char *t )
{
 unsigned long  sy = TIME_EPOCH; /* epoch for us (makes leap calc a snap!) */
 unsigned long  sd = 1;        /* starting julian day in start year */
 unsigned long  st = 0;        /* seconds past midnight */
 unsigned long  ut;            /* user time time - seconds past midnight */
 unsigned long  uy;
 unsigned long  ud = 0;        /* days since 1/1 of user year */
 unsigned long  um;            /* month from user strings */
 char *time;
 char *tok;

 time = strdup( t );           /* gives us something to trash! */
 tok = strtok( time, ":" ); 
 ut = atoi( tok ) * 3600;      /* user hour in seconds */
 tok = strtok( NULL, ":" );
 ut += atoi( tok ) * 60;       /* user min in seconds */

 if( (tok = strtok( NULL, ":" )) )   /* seconds are optional */
  ut += atoi( tok );            /* user time in seconds past midnight */

 tok = strtok( d, "/" );
 if( strlen( tok ) > 2 )       /* yyyy/mm/dd */
  {
   uy = atol( tok );            /* user year */
   tok = strtok( NULL, "/" );
   um = atoi( tok );
   ud = dbm[um-1];        
   tok = strtok( NULL, "/" );
   ud += atoi( tok );
  }
 else                            /* mm/dd/yyyy */
  {
   um = atoi( tok );
   ud = dbm[um-1];        
   tok = strtok( NULL, "/" );
   ud += atoi( tok );            /* add in days for this month */
   tok = strtok( NULL, "/" );
   uy = atoi( tok ); 
  }

 if( um > 2 && (uy % 4 == 0) )          /* works until 2100 */
  ud++;
 
 ut += (ud * SEC_DAY);                 /* seconds past midnight 1/1/uy */
 ut += (uy - sy) * SEC_YEAR;           /* time between start year and uy */
 ut += ((uy - sy)/4) * SEC_DAY;        /* leap seconds beteen epoch and uy */

 free( time );

 return uy < TIME_EPOCH ? SEC_DAY : ut-SEC_DAY;  /* ensure we keep time 0 or positive */
}


/*
-----------------------------------------------------------------------
 Mnemonic: UTtime_s2ts
 Abstract: Converts a string containing both the date and time
           into a timestamp unsinged long integer.
 Parms:    s - pointer to string containing date followed by time.
               the date and time are assumed to be separated by spaces
-----------------------------------------------------------------------
*/
unsigned long UTtime_s2ts( char *s )
{
 char *buf;                    /* work copy - dont trash callers version */
 char *t;                      /* pointer at time in the buffer */

 buf = strdup( s );            /* prevent trashing of user data or core dump */
 
 strtok( buf, " " );           /* skip the first string */
 t = strtok( NULL, " " );
 
 return UTtime_ss2ts( buf, t );   /* do the real work with two buffers */ 
}

/*
------------------------------------------------------------------------------
 Mnemonic: UTtime_now
 Abstract: Generate a timestamp for the current date and time
 Parms:    None.
 Returns:  unsigned long timestamp
------------------------------------------------------------------------------
*/
unsigned long UTtime_now( )
{
 time_t  now;                   /* current time -seconds past... */
 struct tm *ltime;              /* info from local time */
 char tstr[40];                 /* buffer to build time string in */
 char mname[5];                 /* month name; orcale or dba hasnt a clue */
 unsigned long ts; 
 

 now = time( NULL );            /* get and convert current system time */
 ltime = localtime( &now );

 strftime( tstr, 35, "%m/%d/%Y %H:%M:%S", ltime );  /* format it */

 return UTtime_st2ts( tstr );           /* convert time based on our epoch */
}



/* 
	conert a timestamp into an array of itegers: m, d, y, h,, min, s
 */
int *UTtime_ts2i( unsigned long ts )  
{
 int y;                     /* integer conversion variables */
 int m;
 int d;
 int h;
 int min;
 int next_year = SEC_YEAR;  /* number of seconds to knock off of ts for year */
 char buf[100];             /* work buffer */
 int *time;
 int *days;                 /* pointer to dbm array based on leap year or not */


 time = (int *) malloc( sizeof( int ) * 6 );

 y = TIME_EPOCH;               /* calculate the year of the time stamp */
 while( ts >= next_year )      /* while ts is larger than the secs in next yr */
  {                            /* cant do simple division because of leaps */
   y++;                        /* bump year */
   ts -= next_year;            /* knock a year off of ts */

   if( y % 4 )                      /* determine seconds in next year */ 
    next_year = SEC_YEAR;              /* next year is a normal year */
   else
    next_year = SEC_YEAR + SEC_DAY;    /* next year is a leap year */
  }

 days = y % 4 ? dbm : ldbm;          /* account for leap year */

 d = ts / SEC_DAY;      /* number of days since beginning of year (0 based) */

 for( m = 0; m < 12 &&  days[m] <= d; m++ );   /* find the month based on d */

 ts -= d * SEC_DAY;                /* leave seconds past midnight in ts */
 d++;                              /* d can be safely made 1 based now */
 d -= days[m-1];                   /* remove days occuring before this month */

 h = ts / SEC_HR;                  /* hours since midnight */
 ts -= h * SEC_HR;
 min = ts / 60;                    /* minutes past the hour */
 ts -= min * 60;                   /* seconds left in ts */

 time[0] = m;
 time[1] = d;
 time[2] = y;
 time[3] = h;
 time[4] = min;
 time[5] = (int) ts;

 return time;
#ifdef KEEP
 sprintf( buf, "%02d/%02d/%4d %02d:%02d:%02d", m, d, y, h, min, (int) ts );

 return strdup( buf );*/             /* give them a neatly fitted string */
#endif
}


/*
------------------------------------------------------------------------------
 Mnemonic: cvt_i_ts
 Abstract: Converts a timestamp into an array of integer values
 Parms:    timestamp
 Returns:  Pointer to string
------------------------------------------------------------------------------
*/
static int *cvt_ts2iv( time_t ts )
{
	struct tm *cur_time;         /* structure for localtime call to fill in */
	int	*values = NULL;
	
	if( (values = malloc( sizeof( int ) * 6 )) != NULL )
	{
		cur_time = localtime( &ts );  

		values[0] = cur_time->tm_mon;
		values[1] = cur_time->tm_mday;
		values[2] = cur_time->tm_year;
		values[3] = cur_time->tm_hour;
		values[4] = cur_time->tm_min;
		values[5] = cur_time->tm_sec;
	}

	return values;
}                     

/*
------------------------------------------------------------------------------
 Mnemonic: UTtime_ts2dow
 Abstract: Converts a timestamp into a day of week string
 Parms:    timestamp
 Returns:  Pointer to string
------------------------------------------------------------------------------
*/
char *UTtime_ts2dow( unsigned long ts )
{
	char *soup = "SunMonTueWedThuFriSat";
	char *d;
	int *time = NULL;
	int x;

	d = (char *) malloc( sizeof( char ) * 4 );   /* returnable string */
	time = cvt_ts2iv( ts );                       /* cvt to time[0]=month time[1]=day time[2]=year time[3]hr... */

	if( time[2] > 1900 )
 		time[2] -= 1900;                             /* need years past 1900 */

	x = (((time[0] - 1) * 31) + time[1]) + (time[2] * 365);   /* magic */

	if( time[0] > 2 )
		x += (time[2]/4) - ((time[0] * 4) + 23) / 10;
	else
		x += (time[2] -1)/4;

	x = (x % 7) * 3;

	strncpy( d,  soup + x, 3 );
	*(d+3) = (char) 0;

	free( time );

 	return d;
}

/*
------------------------------------------------------------------------------
  Mnemonic: UTtime_ts2s 
  Abstract: Converts one of our timestamps to a date string.
  Parms:    ts - unsigned long time stamp
  Returns:  Pointer to the string containing the date
------------------------------------------------------------------------------
*/
char *UTtime_ts2s( unsigned long ts )
{
 	int *time;                     /* integer conversion variables */
 	char buf[100];                 /* work buffer */

	time = cvt_ts2iv( ts );
	sprintf( buf, "%02d/%02d/%4d %02d:%02d:%02d", time[0], time[1], time[2], time[3], time[4], time[5] );

	free( time );

	return strdup( buf );             /* give them a neatly fitted string */
 
}
