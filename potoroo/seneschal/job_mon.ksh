#!/ningaui/bin/ksh
# ======================================================================== v1.0/0a157 
#                                                         
#       This software is part of the AT&T Ningaui distribution 
#	
#       Copyright (c) 2001-2009 by AT&T Intellectual Property. All rights reserved
#	AT&T and the AT&T logo are trademarks of AT&T Intellectual Property.
#	
#       Ningaui software is licensed under the Common Public
#       License, Version 1.0  by AT&T Intellectual Property.
#                                                                      
#       A copy of the License is available at    
#       http://www.opensource.org/licenses/cpl1.0.txt             
#                                                                      
#       Information and Software Systems Research               
#       AT&T Labs 
#       Florham Park, NJ                            
#	
#	Send questions, comments via email to ningaui_support@research.att.com.
#	
#                                                                      
# ======================================================================== 0a229


#!/ningaui/bin/ksh
# Mnemonic:	
# Abstract:	report some running stats on jobs
#		
# Date:		10 May 2003
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

ustring="[-b bcast-node] [-f] [-F node-format] [-i seconds] [-l logname] [-r] [-W width] [-w filter-string]"

log=$NG_ROOT/site/log/master
what=""
running=0
interval=3000		# tenths of seconds before update 
bcast=""
vlevel=0
tail_count="15000"	# default to tail the lat 15,000 lines
tail_opt=""		# set to other options as needed (like -F)
col_width=5		# width of node columns 

# -------------------------------------------------------------------
while [[ $1 = -* ]]
do
	case $1 in 
		-b)	bcast="$2"; shift;;
		-c)	tail_count="$2"; shift;;
		-f)	running=1; tail_opt="-F";;		# -F catches jump when the log is truncated
		-i)	interval=$(( $2 * 10 )); shift;;
		-l)	log="$2"; shift;;
		-r) 	running=1;;				# running totals for each n minute interval, but not follow mode
		-w)	what=$2; shift;;
		-W)	col_width=$2; shift;;

		-F)	shift;;					# back compat -- we ignore all of these
		-N)	shift;;

		-v)	verbose=1; vlevel=$(( $vlevel + 1 ));;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

ng_dt -i |read now


if [[ $interval -lt 600 ]]		# no more than once per minute
then
	interval=600
fi

if [[ $log != /* ]]
then
	log="$NG_ROOT/site/log/"$log
fi

members="$(ng_members)"

# we tack a space on to the end of the format string so user does not have to
# CAUTION: ast tail does not support -F
tail -n $tail_count $tail_opt $log | awk \
	-v col_width="$col_width" \
	-v interval=${interval:-3000} \
	-v running=$running \
	-v what="$what" \
	-v bcast="$bcast" \
	-v verbose=$vlevel \
	-v now=$now \
	-v members="$members" \
	'
	function setvars( 			name )		# snarf out xxxx(name) vars from the message
	{
		for( i=1; i <= NF; i++ )		
		{
			if( x = index( $(i), "(" ) )
			{
				split( $(i), a, "(" );
				name = substr( a[2], 1, length(a[2])-1 );
				var[name] = a[1];
			}
		}
	}


	# header is timestamp and 5/10/15 min totals; then # failures, total in the hour and error/warn counts
	# following that, we print a count of successful jobs for each node followed by a count of each type
	# type info is value(type-name) format. 
	function header( 			nm, i )
	{
		printf( "\n%9s ", "Day/Time" );
		printf( "%14s %5s ", "5m  10m  15m", "Fail" );
		printf( "%4s  ", "Hr" );
		printf( "Err/Wrn  " );			
	
		sub( " *$", "", lfmt );

		for( i = 0; i < nnode; i++ )
		{
			l = length( col[i] ) - col_width + 1; 
			printf( col_sfmt, substr( col[i], l > 0 ? l : 1 ) );
		}

		printf( "\n" );

		hcount = 30;
	}

	# spit out a line of useful data; see header for description
	function spit( 		i, s15, s10, s5, f15, f10, f5, lhr )
	{
		s15= s10= s5= f15= f10= f5=  0;
		smin = min - 15;
		lhr = hr - 1;
		if( lhr < 0 )
			lhr += 24;

		if( smin < 0 ) 
			smin += 60;

		for( i = 0; i < 15; i++ )
		{
			s15 += jpm_s[smin];
			f15 += jpm_f[smin];
			if( i > 4 )
			{
				s10 += jpm_s[smin];
				f10 += jpm_f[smin];
				if( i > 9 )
				{
					s5 += jpm_s[smin];
					f5 += jpm_f[smin];
				}
			}

			if( ++smin > 59 )
				smin = 0;
		}

		buf = sprintf( "%02d/%02d:%02d:", day,hr, min );
		buf = sprintf( "%s %4d %4d %4d %3d", buf, s5, s10, s15, f15 );
		buf = sprintf( "%s   %4d ", buf, jph_s[hr] );
		buf = sprintf( "%s %3d/%-3d", buf, ecount, wcount ); 
		bbuf = buf;						# broadcast buffer has just front part and then types

		printf( "%s |", buf );					# easy eye seperation

		for( i = 0; i < nnode; i++ )		
		{
			printf( col_dfmt, node_count[col[i]] );
			node_count[col[i]] = 0;					# reset counter after we print it
		}

		bbuf = "";
		for( x in type_count )
		{
			if( type_count[x] )
			{
				printf( "%d(%s) ", type_count[x], x );
				bbuf = sprintf( "%s %d(%s)", bbuf, type_count[x], x );
			}
			type_count[x] = 0;
		}

		printf( "\n" );

		if( bcast )
		{
			colour = crit_colour > 4 ? 1 : (crit_colour > 2 ? 2 : 6);
			cmd = sprintf( "ng_zoo_alert -o %s -c %d -s %s \"%s\"", bcast_dt, colour, bcast, bbuf );
			system( cmd );
			crit_colour--;
		}


		#fflush( );
		hcount--;
		ecount = wcount = crit = 0;
		
	}

	function status(  		a, b )
	{

		if( var["status"] == 0 )		# successful job 
		{
			split( var["job"], a, "." );		# disect the seneschal job name to get job type
			split( a[1], b, "_" );
			type_count[substr(b[1],1,4)]++;
			node_count[var["node"]]++;

			total_s++;
			jpm_s[min]++;
			jph_s[hr]++;	
		}
		else					# job failed
		{
			total_f++;
			jpm_f[min]++;
			jph_f[hr]++;	
		}
	}

	BEGIN {
		col_sfmt = sprintf( "%%%ds ", col_width );
		col_dfmt = sprintf( "%%%dd ", col_width );

		nnode = split( members, a, " " );			# build an xlation table member names to column number
		for( i = 1; i <= nnode; i++ )
			col[i-1] = a[i]; 

		start_snarf = now - 3000;		# where to snarf if not rolling.
		header( );
		if( bcast )
		{
			x = split( bcast, a, ":" );
			bcast = a[1];
			if( x > 1 )
				bcast_dt = a[2];
			else
				bcast_dt = "alert_msg2";		# which drawing thing to target stuff to
		}
	}

	# ------------------------------------- per record processing -----------------------------------------------------
	
	/ng_prs/ { next; }		# they bugger timestamps in log messages; ignore them all 

	{							# set-up for each input record
		if( what && ! index( $0, what ) )		# if user gave us a pattern, skip if pattern not found
			next;
		if( last == $0 )				# skip dup SENE_* records (they should be sequental) 
			next;

		timestamp = $1+0;				# timestamp stuff from the record
		day = substr( $1, 18, 2 )+0;
		hr = substr( $1, 20, 2 )+0;
		min = substr( $1, 22, 2 )+0;

		if( (nmin = min + 1) > 59 )			# next min/hr
			nmin = 0;

		if( (nhr = hr+2) > 23 )
			nhr = 0;

		jpm_s[nmin] = 0;		# clear the counters for the next minute and next hour
		jpm_f[nmin] = 0;
		jph_s[nhr] = 0;
		jph_f[nhr] = 0;

		if( !running && $1+0 < start_snarf )
			next;

		setvars( );			# snarf values
	}


	/\[CRIT/	{ 
		crit++; 
		crit_colour = 4;		# we keep the crit indication lit for ths many go-rounds
		split( $4, a, "[" );
		crit_info = sprintf( "CRIT on %02d @ %02d:%02d: %s %s ", day,hr, min, $3, a[1] );
		cmd = sprintf( "ng_zoo_alert -c %2 -s %s \"%s\"", colour, bcast "_2", crit_info );
		system( cmd );
		if( verbose )
			print;
	}
	/ng_init/	{
		type_count["restart"]++;		# might be nice to know this
	}

	/\[ERROR/	{ 
		ecount++; 
		if( verbose > 1 )
			print;
	}
	/\[WARN/	{ 
		wcount++ 
		if( verbose > 2 )
			print;
	}

	/comp_job:/ {
		status( );
	}

	/SENESCHAL_AUDIT complete/ {
		status( );
		last = $0;
	}

	/SENESCHAL_AUDIT status/ {
		if( var["status"]  )		# track only status failures
			status( );
		last = $0;
	}
	
	{
		if( running )
		{
			if( time < timestamp )
			{
				spit( );
				time = timestamp+interval;
			}

			if( hcount <= 0 )
				header( );
		}
	}

	END {
		spit( );
	}
'

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(job_mon:Seneschal Job Monitor)

&space
&synop	ng_job_mon [-b bcast-node] [-c count] [-f] [-i seconds] [-l logname] [-r] [-w filter-string]

&space
&desc
	&This scans the master log for &ital(seneschal) audit messages and 
	creates a chart of job information that includes:

&break
&beglist
&item	The number of completed jobs during the past 5, 10 and 15 minutes 
&item	The number of failed jobs during the past 15 minutes 
&itam	The number of completed jobs observed during the current and previous hours
&item	The total number of completed jobs observed during the scan.

&space
	&This can be run in scan or follow modes.  In follow mode the master log
	is followed and a status line is written with each change, but no more 
	frequently than every minute. In scan mode, the number of log messages 
	are scanned for audit messages, and a single tally line of information 
	is written to the standard output. If the running option is set (-r) 
	then a message for each time interval is presented as the data is 
	scanned.

&space
&subcat Ouput Syntax
	Each output record written to the sandard output device contains time/date
	information followed by data. The time/date information consists of the 
	day of the month, followed by a slash character and the hour and minute
	following the observation.
&space
	The data consists of three sections.
	The first section, jobs per minute, are written as three values for successfully 
	completed messages, and a single value representing failed jobs. The 
	three successful values are written in &ital(load average) style of jobs 
	completing during the last 5, 10, and 15 minute interval.  The failed job count
	indicates the number of jobs that failed during the most recent 15 minute interval.
&space
	The middle section contains hourly job information. The two columns in this 
	section indicate the number of jobs that have completed during the current 
	hour, and the number of jobs that completed during the previous hour. This 
	section represents only successfully completed jobs. 
&space
	The final section contains totals.  Currently, this section lists only
	the total number of successfully completed jobs that &this has observed
	during its scan over the data. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-b node:var : Supplies the node name, and variable name, that this script should
	broadcast update information to.  The node is expected to be running &lit(ng_parrot)
	which will forward the messages out to all listeners.  The listeners are assumed
	to be W1KW processes which convert messages to update graphical objects using 
	the supplied variable name. If not supplied, the W1KW messages are not sent.
&space
&term	-c n : Supplies the number of log messages (from the end of the log) to scan 
	and collect data from.  This option may be supplied in both scan and follow
	modes. If not supplied, the most receint 15,000 lines are scanned. 
&space
&term	-f : Follow mode.  Following the scan, &this will follow the master log and 
	present data as it is read (not more frequently than once per minute).
	Automatically enables running mode. 
&space
&term	-i sec : Defines the interval between updates when presenting running data. 
	The default is 300 seconds (5 minutes), and can be set as low as 60 seconds. 
&space
&term	-l log : Supplies an alternate log to use. If not supplied, the ningaui 
	master log ($NG_ROOT/site/log/master) is used. 
&space
&term	-r : Running mode. When supplied, all intervals are presented during the 
	scan of the log data rather than just a final line of output.  This option is 
	automatically set when running in follow mode. 
&space
&term	-w pattern : The search pattern to use. Only records in the log containing 
	the supplied pattern will be scanned.  This is generally used to supply a 
	job name, or fragment, to report on a particular job set. If not supplied, 
	all lines of the master log are considered.
&space
&term	-W n : Supplies the column width of the by node counts.  Column header names
	will be truncated to the LAST n characters of each node name. This assumes 
	that node names will be of the form crock00 and thus the last n characters of 
	each name is more useful than the first n.  If not supplied, the default is 3. 
&space
&term	-v : Verbose mode.  In verbose mode, critical error messages are written with 
	the normal output.  If -v -v is supplied, error messages are written, and if 
	three -v options are given, warnings are displayed (in addition to crit and errors).
&space
&term	-w filter : Allows the user to generate statistics only on audit messages that 
	contain the &ital(filter) string. 
&endterms


&space
&see	&ital(seneschal)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	13 May 2003 (sd) : Thin end of the wedge.
&term	19 May 2003 (sd) : Now reports err/warn/crit counts.
&term	22 Jul 2003 (sd) : Fixed bug in by node printing.
&term	17 Dec 2004 (sd) : Avoid looking at records from ng_prs*
&term	15 May 2006 (sd) : Stops when an ng_init message is seen; assume node bounced
		and so should we.
&term	17 Sep 2006 (sd) : Modified to not depend on node names being sequential
		(e.g. crock01, crock03, crock05 are ok).  Added -W option.
&term	03 Dec 2008 (sd) : Corrected the man page. 
&endterms

&scd_end
------------------------------------------------------------------ */

