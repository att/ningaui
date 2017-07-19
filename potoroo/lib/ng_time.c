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

#include	<sys/types.h>
#include	<sys/times.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<string.h>
#include	<time.h>
#include	<stdio.h>
#include	<sfio.h>

#include	<ng_basic.h>
#include	<ng_lib.h>
#include	<ng_ext.h>

#define	DIFF_70_94	8766*24*3600	/* seconds difference between 1970 and 1994 -- unix epoch and ours */

#define MAX_YEAR	2025		/* our limits */
#define MIN_YEAR	1994

					/* flags that describe how we deal with errors and provide back compat */
#define EF_ORIG_ABORT	0x01		/* orignal checks that aborted on error will abort */
#define EF_ALL_ABORT	0x02		/* all traps (original and new) will abort */

void julcal(long, int *, int *, int *);

static int msize[] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 1994 */
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 1995 */
	31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 1996 */
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 1997 */
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 1998 */
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 1999 */
	31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 2000 */
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 2001 */
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 2002 */
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 2003 */
	31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 2004 */
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 2005 */
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 2006 */
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 2007 */
	31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 2008 */
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 2009 */
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 2010 */
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 2011 */
	31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 2012 */
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 2013 */
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 2014 */
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 2015 */
	31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 2016 */
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 2017 */
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 2018 */
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 2019 */
	31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 2020 */
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 2021 */
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 2022 */
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 2023 */
	31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 2024 */
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,		/* 2025 */
	0
};

static float hz = 0;
static int err_handling_flags = EF_ORIG_ABORT;		/* some original function error checking aborted; allow user to turn off */


/* allow user to set/reset the abort on error flags  
	if user sets such that nothing aborts, detected errors (usually in the syntax of a date buffer) are 
	communicated by returning a 0 value.
*/
void ng_time_err_action( int abort_setting )
{
	switch( abort_setting )
	{
		case 0: 	err_handling_flags &= ~(EF_ORIG_ABORT | EF_ALL_ABORT);	break; 	/* nothing aborts */
		default:	err_handling_flags |= EF_ORIG_ABORT | EF_ALL_ABORT; break;	/* everything aborts */
	}
}


/* return true if date seems legit */
static int vet_date( int y, int m, int d )
{
	if( y < 50 )
		y += 2000;
	if( y < 100 )
		y += 1900;

	if( y < MIN_YEAR || y > MAX_YEAR )
	{
		sfprintf( sfstderr, "ng_time: invalid date for conversion (year): %02d/%02d/%02d\n", y, m, d );
		return 0;
	}

	if( m < 1 || m > 12 )
	{
		sfprintf( sfstderr, "ng_time: invalid date for conversion (month): %02d/%02d/%02d\n", y, m, d );
		return 0;
	}

	if( d < 1 || d >  msize[((y-MIN_YEAR)*12)+(m-1)] )
	{
		sfprintf( sfstderr, "ng_time: invalid date for conversion (day): %02d/%02d/%02d\n", y, m, d );
		return 0;
	}

	return 1;
}

/* return true if date seems legit */
static int vet_time( int h, int m, int s )
{
	if( h < 0 || h > 23 )
	{
		sfprintf( sfstderr, "ng_time: invalid time for conversion (hour): %02d:%02d:%02d\n", h, m, s );
		return 0;
	}

	if( m < 0 || m > 59 )
	{
		sfprintf( sfstderr, "ng_time: invalid time for conversion (min): %02d:%02d:%02d\n", h, m, s );
		return 0;
	}

	if( s < 0 ||  s > 59 )
	{
		sfprintf( sfstderr, "ng_time: invalid time for conversion (sec): %02d:%02d:%02d\n", h, m, s );
		return 0;
	}

	return 1;
}


void
ng_time(Time *tp)
{
	struct tms t;

	/* following casts to float are anal but correct */
	if(hz == 0){
		hz = (float)sysconf(_SC_CLK_TCK);
#ifdef CLK_TCK
		if(hz < 0)
			hz = (float)CLK_TCK;		/* if not defined we will get a coredump if sysconf gives us nothing back */
#endif
	}
	tp->real = (float)times(&t)/hz;
	tp->usr = (float)t.tms_utime/hz;
	tp->sys = (float)t.tms_stime/hz;
}


ng_timetype
ctimeof(int hhmmsst)
{
	long h, m;
	ng_timetype r;

	h = hhmmsst/100000;
	m = (hhmmsst/1000)%100;
	r = hhmmsst%1000 + m*600 + h*36000;

	if( !vet_time( h, m, (hhmmsst%1000)/10 ) )
	{
		if( err_handling_flags & EF_ALL_ABORT )
			exit( 1 );
		else
			return 0;
	}

	return r;
}

ng_timetype
etimeof(int mmmmsst)
{
	ng_timetype r, m;

	m = mmmmsst/1000;
	r = mmmmsst - 1000*m + m*600;
	return r;
}

ng_timetype
cal2jul(long yyyymmdd)
{
	long m,d,y,ya,c;
	ng_timetype julian;

        if( yyyymmdd < 101 )   		/* month & day must be at least 1 */
	{				/* AMA year 00101 -- jan 1 2000/2010... */
		sfprintf( sfstderr, "invalid [yy]yymmdd for cal2jul conversion: %I*d\n", Ii(yyyymmdd) );
		if( err_handling_flags & EF_ALL_ABORT )
			exit( 1 );
		else
         		return 0;             
	}

	d = yyyymmdd % 100;
	yyyymmdd /= 100;
	m = yyyymmdd % 100;
	y = yyyymmdd/100;
	if(y < 10){
		time_t curtime;
		struct tm tinfo;
		static int cutoff = 0;

		if(cutoff == 0){
			curtime = time(0);
			gmtime_r(&curtime, &tinfo);
			cutoff = tinfo.tm_year + 1900;		/* true year */
			cutoff -= 6;				/* assume date in -6..+4 */
		}
		for(y += 1970; y < cutoff; y += 10)
			;

	} else if(y < 100){
		if(y < 50)
			y += 2000;	/* assume 20yy */
		else
			y += 1900;
	}

	if( !vet_date( y, m, d ) )
	{
		if( err_handling_flags & EF_ALL_ABORT )
			exit(1);
		else
			return 0;
	}

        y = y + (m > 2?  0: -1);
        m = m + (m > 2? -3: 9);
        c = y / 100;
        ya = y - 100 * c;
        julian =  146097*c/4 + 1461*ya/4;
        julian += (153*m+2)/5 + d - 728235;	/* relative to 1/1/94 */

        return julian;
}

ng_timetype
ng_timestamp(char *buf)
{
	int yr, mo, dy, hr, m, s;
	ng_timetype t;

	t = 0;
	while(*buf == ' ')
		buf++;
	if(strchr(buf, ':')){
		if(sscanf(buf, "%04d/%02d/%02d %02d:%02d:%02d", &yr, &mo, &dy, &hr, &m, &s) != 6){
			if(sscanf(buf, "%04d%02d%02d %02d:%02d:%02d", &yr, &mo, &dy, &hr, &m, &s) != 6){
				fprintf(stderr, "%s: bad timestamp '%s'; must be yyyy/mo/dy hr:min:sec or yyyymmdd hr:min:sec\n", argv0, buf);
				if( err_handling_flags & EF_ORIG_ABORT )
					exit(1);
				else
					return 0;
			}
		}
	} else if(strchr(buf, '/')){
		if(sscanf(buf, "%04d/%02d/%02d", &yr, &mo, &dy) != 3){
			fprintf(stderr, "%s: bad timestamp '%s'; must be yyyy/mo/dy\n", argv0, buf);
			if( err_handling_flags & EF_ORIG_ABORT )
				exit(1);
			else
				return 0;
		}
		hr = m = s = 0;
	} else if(strlen(buf) == 5){
		if(sscanf(buf, "%02d%03d", &yr, &dy) != 2){
			fprintf(stderr, "%s: bad timestamp '%s'; must be yyddd\n", argv0, buf);
			if( err_handling_flags & EF_ORIG_ABORT )
				exit(1);
			else
				return 0;
		}
		yr += 1900;
		if(yr < 1950)
			yr += 100;
		t = (cal2jul(yr*10000 + 101) + dy)*DAY;
	} else if(strlen(buf) == 6){
		if(sscanf(buf, "%04d%02d", &yr, &mo) != 2){
			fprintf(stderr, "%s: bad timestamp '%s'; must be yyyymm\n", argv0, buf);
			if( err_handling_flags & EF_ORIG_ABORT )
				exit(1);
			else
				return 0;
		}
		dy = 1;
		hr = m = s = 0;
	} else if(strlen(buf) == 8){
		if(sscanf(buf, "%04d%02d%02d", &yr, &mo, &dy) != 3){
			fprintf(stderr, "%s: bad timestamp '%s'; must be yyyymmdd\n", argv0, buf);
			if( err_handling_flags & EF_ORIG_ABORT )
				exit(1);
			else
				return 0;
		}
		hr = m = s = 0;
	} else {
		if(sscanf(buf, "%04d%02d%02d%02d%02d%02d", &yr, &mo, &dy, &hr, &m, &s) != 6){
			fprintf(stderr, "%s: bad timestamp '%s'; must be yyyymmddhhmmss\n", argv0, buf);
			if( err_handling_flags & EF_ORIG_ABORT )
				exit(1);
			else
				return 0;
		}
	}

	/* lazy but correct */
	if(t == 0)
	{
		if( (t = cal2jul(yr*10000 + mo*100 + dy)) <= 0 || !vet_time( hr, m, s ) ) 	/* we now break this apart to return 0 if it does not vet */
			return 0;
		
			t = (t * DAY) + hr*36000 + m*600 + s*10;
	}

	/* return local time */
	return t;
}

/* convert string in buf (yyyymmddhhmmss) to unix integer time assuming that the timestamp in the buffer is 
   loacal time. we convert the buffer to ningaui time which is local time, then adjust for the timezone and 
   finally convert it to unix time.  
*/
ng_timetype ng_utimestamp( char *buf )
{       
        static ng_timetype tz = 0;	/* timezone offset in seconds East (pos) or West (neg) of the meridian */
	static int have_tz = 0;		/* value in tz has been calculated */
        ng_timetype ntime;		/* ningaui time */

	if( ! have_tz )				/* compute tz difference only once, more effort because sun does not support tinfo->tm_gmtoff */
	{
		struct tm	*tinfo;
		time_t curtime; 		/* current time */
		int ghr;			/* gmt hour and day */
		int lday;
		int lhr;			/* local hr and day */
		int gday;

		have_tz++;
		curtime = time( 0 );
		tinfo = localtime( &curtime );
		/* wed rather do this, but sun's implementation doesnt support it: tz = tinfo->tm_gmtoff; */		/* what we add to zulu to get local time */

		lhr = tinfo->tm_hour;				/* so we compute it via these hoops */
		lday = tinfo->tm_mday;
		tinfo = gmtime( &curtime );
		ghr = tinfo->tm_hour;
		gday = tinfo->tm_mday;

		if( gday > lday )
			ghr += 24;
		else
			if( gday < lday )
				lhr += 24;
		tz = 3600 * (lhr - ghr);		
	}

        ntime = (ng_timestamp( buf )/10) - tz; 		/* convert buffer to ningaui time -- convert to seconds and adjust for timezone difference */
        return ntime > 0 ? ntime + DIFF_70_94 : 0;      /* convert ningaui time to unix time, but return 0 if timestamp had issues  */
}

/* convert buffer (YYYYMMDDHHMMSS) into gmt assuming that the buffer time is a gmt value and not local time */
ng_timetype ng_utimestamp_gmt( char *buf )
{       
        ng_timetype ntime;              /* ningaui time */

        ntime = ng_timestamp( buf );                   	 	/* convert buffer to ningaui time */
        return ntime > 0 ? ((ntime/10) + DIFF_70_94) : 0;	/* convert ningaui time to unix time, but return 0 if timestamp had error */
}       
                  
char *
ng_stamptime(ng_timetype t, int pretty, char *buf)
{
	int yr, mo, dy, hr, m, s;

	julcal((long)(t/DAY), &yr, &mo, &dy);
	t = t%DAY;
	hr = t/36000;
	m = (t/600)%60;
	s = (t/10)%60;

	if( !vet_date( yr, mo, dy ) || !vet_time( hr, m, s ) )
	{
		fprintf( stderr, "timestamp out of range: %I*d => %04d/%02d/%02d %02d:%02d:%02d\n", Ii(t), yr, mo, dy, hr, m, s );

		if( err_handling_flags & EF_ALL_ABORT )
			exit(1);
		else
			sprintf( buf, "INVALID" );

		return buf;
	}

	switch(pretty)
	{
	default:
	case 0:		sprintf(buf, "%04d%02d%02d%02d%02d%02d", yr, mo, dy, hr, m, s); break;
	case 1:		sprintf(buf, "%04d/%02d/%02d %02d:%02d:%02d", yr, mo, dy, hr, m, s); break;
	case 2:		sprintf(buf, "%04d%02d%02d", yr, mo, dy); break;
	case 3:		sprintf(buf, "%04d%02d%02d %02d:%02d:%02d", yr, mo, dy, hr, m, s); break;
	}
	return buf;
}

void
julcal(long jul, int *year, int *month, int *day)
{
	int i;

	for(i = 0; msize[i]; i++){
		if(jul < msize[i])
			break;
		jul -= msize[i];
	}
	if(year)
		*year = 1994 + i/12;
	if(month)
		*month = i%12 + 1;
	if(day)
		*day = jul+1;
}

ng_timetype
dt_ts(long ymmdd, long hhmmsst)
{
	ng_timetype ts;
	ng_timetype sts = 0;

	if( (ts =  cal2jul(ymmdd) * 24 * 3600 * 10) == 0 )
		return 0;

	if( hhmmsst )					/* dont check if 0 */
	{
		if( (sts = ctimeof(hhmmsst)) == 0 )
			return 0;
	}

	return ts + sts;
	/*return cal2jul(ymmdd)*24*3600*10 + ctimeof(hhmmsst);     original before adding error checking */
}

ng_timetype
ng_now(void)
{
	time_t curtime;                    /* current time */
	struct tm tinfo;                   /* time information structure */

	curtime = time(0);
	localtime_r(&curtime, &tinfo);                   /* get current date/time */
	return cal2jul((tinfo.tm_year + 1900)*10000 + (tinfo.tm_mon+1)*100 + tinfo.tm_mday)*DAY
		+ 10*(tinfo.tm_hour*3600 + tinfo.tm_min*60 + tinfo.tm_sec);
}


/* 	timezone delclared extern by time.h (linux)
	trying to second guess what purpose this function serves:
		tz seems to be a string like: EST5EDT (based on things that call this function)
		the funciton uses local time to determine if it is was daylight saving time at 
		the time of 't', so t IS important.  It then calculates the number of hours
		EAST of greenwich based on the daylight setting.

	This function is old 'gecko' code and probably was implemented to support parsing of 
	data that recorded time in local time with odd zone information rather than recording
	time in zulu. 
*/
int
ng_tzoff(char *tz, long t)
{
	static char e[100];		/* must be static because putenv may not make a copy of it */
	time_t ut;
	struct tm tinfo;
	char *s;
	int utc_off;

	s = getenv("TZ");			/* save original tz value */
	sprintf(e, "TZ=%s", tz);		/* fake out localtime with tz info from caller */
	putenv(e);
	tzset();

	ut = t/10 + UNIX_OFFSET;		/* convert ningaui time to unix time */
	localtime_r(&ut, &tinfo);		/* sets timezone (seconds west of utc) */

/* cygwin cannot handle either case */
#if ! defined ( OS_CYGWIN )
#if	defined (OS_IRIX) || defined (OS_SOLARIS)
	if( tinfo.tm_isdst > 0 )
		timezone -= 3600;   /*= altzone; */  /* one hour earlier when in daylite mode */

	utc_off = 24 - timezone/3600;
#else
	tinfo.tm_gmtoff = abs (tinfo.tm_gmtoff );	/* comes back as - for west and IS corrected for timezone! */
/*
	if( tinfo.tm_isdst > 0 )
		tinfo.tm_gmtoff -= 3600; */   /*= altzone; */  /* one hour earlier when in daylite mode */


	utc_off = 24 - (tinfo.tm_gmtoff/3600);		/* convert to # hours EAST of g */
#endif
#endif


	if(s){					/* restore so caller's time oriented calls will work properly */
		sprintf(e, "TZ=%s", s);
		putenv(e);
		tzset();
	}
	else				/* TZ was not previously defined */
	{
		*e = 0;
		putenv( "TZ" );		/* unset it */
	}

	return utc_off;
}

/* deprecated --------------------------------------------------------------- */
static char *xng_gettimes( )
{
 char *bptr = NULL;                 /* pointer to buffer to return string in */
 time_t curtime;                    /* current time */
 struct tm tinfo;                   /* time information structure */


 curtime = time( NULL );
 localtime_r( &curtime, &tinfo );                   /* get current date/time */

 bptr = (char *) ng_malloc( sizeof( char ) * 10, "ng_gettd, hms, bptr" );
 sprintf(bptr, "%02d%02d%02d", tinfo.tm_hour, tinfo.tm_min, tinfo.tm_sec );
 return( (void *) bptr );
}


static char *xng_getdates( )                /* get date string */
{
 char *bptr = NULL;                 /* pointer to buffer to return string in */
 time_t curtime;                    /* current time */
 struct tm tinfo;                   /* time information structure */


 curtime = time( NULL );
 localtime_r( &curtime, &tinfo );                   /* get current date/time */
 bptr = (char *) ng_malloc( sizeof( char ) * 12, "ng_gettd, date, bptr" );

 sprintf(bptr, "%04d%02d%02d", tinfo.tm_year + 1900, tinfo.tm_mon+1,
                   tinfo.tm_mday);
 return( bptr );
}


static char *xng_getdts( )        /* get date and time into a string */
{
 char *bptr = NULL;                 /* pointer to buffer to return string in */
 time_t curtime;                    /* current time */
 struct tm tinfo;                   /* time information structure */


 curtime = time( NULL );
 localtime_r( &curtime, &tinfo );                   /* get current date/time */

 bptr = (char *) ng_malloc( sizeof( char ) * 22, "ng_gettd, hms, bptr" );
 sprintf(bptr, "%04d%02d%02d%02d%02d%02d",
                   tinfo.tm_year + 1900, tinfo.tm_mon + 1, tinfo.tm_mday,
                   tinfo.tm_hour, tinfo.tm_min, tinfo.tm_sec );
 return( bptr );
}
/* --------------------------------------------------------------------------------*/

/* return the day of the week (sun=0) for ts; if ts is 0, then for current time */
int ng_dow( ng_timetype ts )
{
	int soup[] = { 0, 0,31,59,90,120,151,181,212,243,273,304,334 };	/* sum of normal days before current month */
	char date[100];
	int y;				/* conversion of timestamp into components */
	int m;
	int d;
	int y19;			/* years since 1900, before date's year */

	if( !ts )
		ts = ng_now( );

	ng_stamptime( ts, 1, date );		/* yyyy/mm/dd hh:mm:ss to date buffer */
	y = atoi( date );
	y19 = y - 1900;
	m = atoi( &date[5] );
	d = atoi( &date[8] ) + soup[m];		/* days since beginning of year in date, not counting leap day if there */
	

	d += y19 * 365;					/* add in normal days since 1900; before date year */
	d += (y19-1)/4;					/* add in leap days since 1900, not including date year  */

	if( m > 2 ? ( y % 4 == 0 ? 1 : 0) : 0 )		/* add leap day for date's year if after feb and valid leap year */
		if( (y % 100) || (y % 400) == 0 )	/* centry years must also be divisible by 400 */
			d++;					

	return d % 7;
}

/* ------------ SELF CONTAINED DOCUMENTATION SECTION -----------------
&scd_start
&doc_title(ng_time:Ningaui Date And Time Related Routines)

&space
&synop
	#include <ningaui.h>
&break	#include <ng_ext.h>
&space	void ng_time(Time *tp);
&break	ng_timetype ng_now(void);
&break	ng_timetype ctimeof(int t);
&break	ng_timetype etimeof(int t);
&break	ng_timetype cal2jul(long yyyymmdd);
&break	char *ng_stamptime(ng_timetype t, int pretty, char *buf);
&break	ng_timetype ng_timestamp(char *buf);
&break	ng_timetype ng_utimestamp( char *buf );
&break	ng_timetype ng_utimestamp_gmt( char *buf );
&break	void julcal(long jul, int *year, int *month, int *day);
&break	ng_timetype dt_ts(long ymmdd, long hhmmsst);
&break  ng_dow( ng_timetype ts );
&break	ng_time_err_action( bool on );
&break	void ng_time_err_action( int abort_setting );

&space
&desc
	The &ital(ng_time()) function fills in the structure pointed to
	by tp with the current user, sysstem and real time values.
	These values include any time used by child processes that
	were started by the process and waited on.

&space
	&ital(Ctimeof(^)) converts &ital(t) into tenths of seconds.
	&ital(t) is assumed to contain the time to convert
	in the format of hours, minutes, seconds, and tenths of seconds.
&space
	The &ital(etimeof(^)) function will convert &ital(t) into
	tenths of seconds.  &ital(t) is assumed to contain a value in
	the format of minutes, seconds, and tenths of seconds.
&space
	The &ital(ng_dow(^)) function will return the day of the week,
	where Sunday is 0, for the timestamp passed in. If timestamp is 
	0, then the day of the week for the current date is returned. 
&space
	&ital(Cal2jul(^)) returns the difference, in days,
	between &ital(yyyymmdd) and January 1, 1994.
	&ital(ymmdd) contains the year, month and day values as a single
	long integer. &ital(year) should be a 4 digit year, but a single digit
	is treated as the last digit and is interpreted as the closest match
	to the current date ( within -6..+4 years).
&space
	The &ital(ng_timestamp(^)) function converts a buffer containing
	a date and time string into a ng_timetype  value.
	The format of the string in the buffer &ital(buf) is assumed to be
        "yyyymmddhhmmss" or "yyddd" or "yr/mo/dy" or "yr/mo/dy hr:mn:sc" or "yrmody hr:mn:sc".
	&ital(ng_stamptime(^)) will convert the long integer &ital(t) into
	a date and time string into different formats selected by the value of &ital(pretty):.
	0: "yyyymmddhhmmss",
	1: "yr/mo/dy hr:mn:sc",
	2: "yyyymmdd",
	3: "yrmody hr:mn:sc".
&space
	The &ital(ng_utimestamp(^)) function does the same kind of conversion as done by 
	&ital(ng_timestamp) except that the value returned is in UNIX time (time()) rather than 
	ningaui time. The time string is assumed to be local time rather than zulu (GMT). 

&space
	The &ital(ng_utimestamp_gmt(^)) function converts the time string in the buffer
	into a unix timestamp as is done in &lit(ng_utimestamp.)
	However, the buffer time is assumed to be a zulu (GMT) time and not a local time. 
	If the environmen variable TZ is unset, this function will return the same result
	as &lit(ng_utimestamp().) The purpose of this function is to allow a programme to 
	convert zulu time strings without having to clear the TZ environment variable. Setting
	the TZ environment variable to null will affect other time conversions including the 
	timestamp that ng_bleat emits. 

&space	
	The &ital(ng_utimestamp(^)) function will convert the same strings as are 
	converted by  &ital(ng_timestamp(^)) into the appropriate UNIX (time()) 
	integer timestamp (seconds pas the epoch which is generally accepted as 
	January 1, 1970). 
&space
	The &ital(ng_julcal(^)) function will convert the number in &ital(jul)
	into year, month, and day values.  &ital(jul) is assumed to be
	a value representing the number of days past
	January 1, 1994. A NULL pointer can be supplied for any of the latter
	three parameters if the caller does not wish to receive that
	particular value from the function.

&space
	&ital(ng_dt_ts(^)) returns a long integer containing the difference,
	in tenths of seconds, between midnight January 1, 1994 and the
	date and time values passed to the function.

&space
	&ital(ng_now(^)) returns a long integer containing the difference,
	in tenths of seconds, between midnight January 1, 1994 and now.

&space
	&ital(ng_time_err_action(^)) allows the user to set the action taken 
	when an error in the user supplied timestamp, or time string, is detected. 
	By default, the original functionality caused the process to exit
	when an error was detected in a time string, but no error was reported
	and no action was taken if the timestamp value passed to 
	these functions was not legitimate.  To maintain backward compatibility, 
	the default action (exit) is still taken for badly formatted user strings. 
	If this function is invoked with a non-zero value, all errors will cause 
	the process to exit. 
&space
	If invoked with a 0, an error message will be generated to stderr, and the return 
	value will contain an obviously bad value.  In the case of conversion from string 
	to timetype, the return value will be zero.  If the conversion 
	was from timetype to string, the string will be set to "INVALID" to indicate
	an error. 
&space
&subcat Error Handling
	The original implmentation of the ningaui time interface did mimimal error 
	checking and under circumstances the process was exited with a non-zero 
	return code if an error was detected by either the ng_timestamp() or 
	ng_stamptime() functions.  

	The implementation has been augmented such that more error checking
	(e.g. date range and hour, minute, second validation) is performed. 
	When an error is detected, the original behaviour in ng_stamptime() 
	and ng_timestamp() (exit) is still honored. All other functions
	that detect an error will return a zero (0). 
&space
	Using the &lit(ng_time_err_action(^)) function, the user programme can 
	cause all functions to exit on error, or can cause all functions to 
	return zero on error.  To cause an exit by all functions, a one (1) 
	should be passed to ng_time_err_action(). Passing a zero (0) will cause
	all of the time functions to return a 0 on error. 

&space	
	If the ng_time_err_action() is never invoked, the original behavour 
	is mixed with the newly added checks that by default return 0.  It is 
	recommended that all user programmes invoke this function in order to 
	establish a consistant return pattern.

&space
&exit	The &ital(ng_timestamp(^)) function will cause the process to exit if
	the buffer does not contain the expected number of parameters
	for the sscanf function to parse.


&space
&examp
&indent
.nf
   #include <ningaui.h>
   #include <ng_ext.h>

   long time = 1405000;          / * 2:05:00.0 pm * /
   long mins = 44253;            / * 44 min, 25.3 seconds * /

   long time_secs;               / * time in tenths of seconds * /
   long mins_secs;               / * minutes in tenths of seconds * /

   Time start_t;                 / * time from ng_time call * /
   Time end_t;                   / * time from ng_time call * /


   ng_time( &start_t );            / * get starting time * /
   time_tsecs = ctimeof( time );   / * convert time to tenths of seconds * /
   time_mins  = etimeof( mins );   / * convert minutest to thenths * /
   ng_time( &end_t );              / * get end time * /

   printf( "Time required by process: user=%.1fs system=%.1fs\n",
            end_t.usr - start_t.usr, end_t.sys - start_t.sys );

.fo
&uindent

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	?? xxx 1997 (ah) : Original Code
&term	1 Oct 1997 (sd) : SCD documentation added.
&term	6 Oct 1997 (sd) : Added ng_getdt() function.
&term	16 Jun 1999 (sd) : Corrected Feb 2000 to have 29 days.
&term	15 Jul 1999 (jrsh) : Corrected ng_tzoff to use different timezone.
&term	10 Aug 1999 (sd) : cal2jul now returns 0 if a date less than 
	00101 is passed in (ama date for 1/1/2000.
&term	28 Mar 2001 (sd) : Converted from Gecko.
&term	02 May 2002 (sd) : Corrected tzoffset to unset TZ if it was not set before the call.
	Also converted buffer to static declaration as putenv may not copy the buffer 
	depending on the lib version used.
&term	25 Mar 2003 (sd) : Converted timezone global ref to use struct returned by local time
&term	23 Feb 2004 (sd) : Corrected doc errors.
&term	31 Jul 2005 (sd) : Changes to prevent gcc warnings.
&term	14 Nov 2005 (sd) : CLK_TCK Not defined under gcc 4.0.  We do not deal with it.
&term	15 Jan 2007 (sd) : Extended the julian mapping table through 2025. Max date ng_dt -p can handle is 12/31/2025 23:59:59.
&term	23 Jsn 2007 (sd) : Added the ng_dow() function. 
&term	03 Oct 2007 (sd) : Corrected man page.
&term	15 Aut 2008 (sd) : Added string to unix conversion; 
&term	27 Aug 2008 (sd) : Added additional error checking and handling.
&term	15 Oct 2008 (sd) : Added utimestamp to convert from character string to unix time.
&term	30 Oct 2008 (sd) : Added utimestamp_gmt to convert a zulu time string to unix time. Also corrected for compile 'bugs' on sun.
&term	25 Feb 2009 (sd) : Corrected printf() call using I*d to pass both size of variable and value.
&term	07 May 2009 (sd) : Corrected problem with utimestamp functions not returning 0 if bad time entered. 
&endterms
&scd_end
*/
