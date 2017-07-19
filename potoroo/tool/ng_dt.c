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
  ---------------------------------------------------------------------------------
  Mnemonic:	ng_dt - date time for Ningaui
  Abstract: 	This programme provides simple date/time string/stamp generation
		and conversion capabilities from the command line. Sse SCD for 
		complete information.
  ---------------------------------------------------------------------------------
*/
#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<time.h>
#include	<string.h>

#include	<sfio.h>

#include	"ningaui.h"
#include	"ng_dt.man"

#define	DIFF_70_94	8766*24*3600	/* seconds difference between 1970 and 1994 -- unix epoch and ours */

int
main(int argc, char **argv)
{
	ng_timetype t;
	ng_timetype tz;			/* timezone adjustment for -u */
	time_t 	hw_time;		/* need this to be the size defined for the hardware as passing &t does not work */
	struct	tm	*tspec;		/* time specifics from system time functions */
	enum { o_none, o_tointernal, o_date, o_pdate, o_ctime, o_unix, o_update, o_udate, o_uctime, o_ts2dow, o_str2dow } op = o_none;
	int i;
	char buf[NG_BUFFER];


	ARGBEGIN{
	case 'c':	op = o_ctime; break;
	case 'd':	op = o_date; break;
	case 'i':	op = o_tointernal; break;
	case 'p':	op = o_pdate; break;
	case 'u':	op = o_unix; break;
	case 'P':	op = o_update; break;		/* unix time to purdy date */
	case 'D':	op = o_udate; break;		/* unix time to yyyymmddhhmmsstt */
	case 'C':	op = o_uctime; break;		/* unix time to ctime format */
	case 'w':	op = o_ts2dow; break;		/* ningaui time to day of week */
	case 'W':	op = o_str2dow; break;		/* ningaui time to day of week */
	case 'v':	OPT_INC(verbose); break;
	default:
usage:
			sfprintf( sfstderr, "Usage:\t%s [-v] -i|W [y/m/d h:m:s | yyddd | yyyymmddhhmmss]\n", argv0);
			sfprintf( sfstderr, "\t%s [-v] -p|d|c|w\n", argv0);
			sfprintf( sfstderr, "\t%s [-v] -P|D|C\n", argv0);
			exit(1);
	}ARGEND

	switch(op)
	{
	case o_tointernal:
	case o_str2dow:
		if(argc == 0)
			t = ng_now();
		else {
			buf[0] = 0;
			/* quadratic in args length and i don't care! */
			for(i = 0; i < argc; i++){
				strcat(buf, " ");
				strcat(buf, argv[i]);
			}
			t = ng_timestamp(buf);
		}

		if( op == o_tointernal )
			sfprintf( sfstdout, "%*Id\n", sizeof( t ), t );
		else
			sfprintf( sfstdout, "%d\n", ng_dow( t ) );		/* to day of the week then out */
		break;

	case o_unix:
		if(argc == 0)
			hw_time = time( 0 );				/* current case is easy; just send back time result */
		else
		{
			tz = time(0) - ((ng_now()/10)+DIFF_70_94);	/* compute how far ahead zulu is from local time (ng_now()) */
			t = strtoull(argv[0], 0, 10);			/* ng_dt -d timestamp to convert (user time) */
			hw_time = tz+ (t/10 + DIFF_70_94);		/* cvt user time to sec, and add diff between 1970 and 1994 and tz adjust */
		}

		sfprintf(sfstdout, "%I*d\n", Ii(hw_time) );
		break;


	case o_uctime:
	case o_update:
	case o_udate:
		if( argc == 0 )
		{
			hw_time = time( NULL );
		}
		else
		{
			hw_time = strtoull( argv[0], 0, 10 );
		}

		if( op == o_uctime )
			sfprintf( sfstdout, "%s", ctime( &hw_time ) );		/* ctime formats with \n */
		else	
		{
			tspec = localtime( &hw_time );				/* get time specifics for the integer */
			if( op == o_udate )
				printf( "%4d%02d%02d%02d%02d%02d\n", tspec->tm_year + 1900, tspec->tm_mon+1, tspec->tm_mday, tspec->tm_hour, tspec->tm_min, tspec->tm_sec );
			else
				printf( "%4d/%02d/%02d %02d:%02d:%02d\n", tspec->tm_year + 1900, tspec->tm_mon+1, tspec->tm_mday, tspec->tm_hour, tspec->tm_min, tspec->tm_sec );
		}
		break;

	case o_ctime:
	case o_date:
	case o_pdate:
	case o_ts2dow:
		if(argc == 0)
			t = ng_now();
		else
			t = strtoull(argv[0], 0, 10);
		if( op == o_ts2dow )
			sfprintf( sfstdout, "%d\n", ng_dow( t ) );
		else
		if(op == o_ctime) {
			t = t/10 + DIFF_70_94;  			/* cvt to sec, and add diff between 1970 and 1994 */
			hw_time = t;						/* scale down if time_t is not 64 bit */
			sfprintf(sfstdout, "%s", asctime( gmtime(&hw_time ))); 	/* must pass pointer to time_t */
		} else {
ng_bleat( 1, "t=%I*d", Ii(t) );
			ng_stamptime(t, op == o_pdate, buf);
			sfprintf( sfstdout, "%s\n", buf);
		}
		break;

	case o_none:
		goto usage;
	default:
		sfprintf( sfstderr, "%s: internal error: op=%d\n", argv0, op);
		exit(1);
		break;
	}

	exit(0);
}

/*
#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&doc_title(ng_dt:convert between date/time formats)

&synop  ng_dt [-v] -i|W [y/m/d h:m:s | yyddd | yyyymmddhhmmss]
&break	ng_dt [-v] -p|d|c|w [ningaui-timestamp]
&break	ng_dt [-v] -P|D|C [ningaui-timestamp]

&desc	&ital(Ng_dt) converts between the Ningaui internal numeric timestamp
	(the number of 1/10th's of seconds since 1994/1/1 00:00:00 UTC) and
	a variety of printed formats. All printed formats are interpreted as local time.
	A missing date/time parameter is interpreted as a conversion of the current 
	time.  
&space
	For each of the conversion options -c, -p, and -d, there are upper case options
	(C, P, and D) which assume that the timestamp parameter is a UNIX timestamp
	and not a ningaui internal timestamp. 

&opts   &ital(Ng_dt) unserstands the following options:

&begterms
&term 	-c : convert the numeric internal timestamp into &ital(ctime)(3) format.
&space
&term	-C : Convert the numeric UNIX timestamp to &ital(ctime)(3) format. 
&space
&term 	-d : convert the numeric internal timestamp into the format &ital(yyyymmddhhmmss).
&space
&term 	-D : convert the numeric UNIX timestamp into the format &ital(yyyymmddhhmmss).
&space
&term 	-i : interpret the arguments as a date and time and output the corresponding internal timestamp.
&space
&term 	-p : convert the numeric internal timestamp into the format &ital(y/m/d h:m:s). If a parameter is supplied
&space
	the parameter is assumed to be the internal timestamp to convert.
&term	-P : Convert the UNIX timestamp into the format &ital(y/m/d h:m:s). 
&space
&term	-u : Print time as seconds since the UNIX epoch (the UNIX timestamp). Time is zulu (GMT) time.  
&space
&term	-w : convert the numeric internal timestamp into the day of the week that the date falls on. The 
	output is a number 0 through 7 inclusive where 0 is Sunday. If the timestamp parameter is omitted then 
	the current date is assumed.
&space
&term	-W : Same as -w, except the command line argument is expected to be a string in the form of: yyyymmddhhmmss,
	or yydd, or y/m/d h:m:s. 
	If the time argument is omitted, then the current date is assumed. 
&space
&term 	-v : be chatty (verbose). The more -v options, the chattier.

&endterms

&exit	An exit code of 1 indicates a format conversion error.

&mods	
&owner(Scott Daniels)
&lgterms
&term	09 Oct 1997 (ah) : Original code.
&term   06 Oct 1999 (sd) : Corrected -c option; changed localtime to gmtime call.
&term	16 Apr 2001 (sd) : Conversion from Gecko.
&term	18 Oct 2004 (sd) : Added a time_t var to use when computing time with -c option; needed with 64 bit ng_timetype.
&term	05 Jan 2005 (sd) : Corrected print issue for the -i option. 
&term	21 Jul 2005 (sd) : The -u option now puts out time in zulu time.
&term	16 Apr 2006 (sd) : Added the -P, -C and -D options to support conversion of UNIX time to human readable.
&term	15 Jan 2007 (sd) : Corrected problem reading binary time from command line. 
&term	23 Jan 2007 (sd) : Added -w and -W options to convert string or ningaui time stamp to day of the week.
&term	20 Nov 2008 (sd) : Corrected manual page. 
&endterms
&scd_end
*/
