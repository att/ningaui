#!/usr/common/ast/bin/ksh
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


#!/usr/common/ast/bin/ksh
# Mnemonic:	rota (version 2)
# Abstract:	Same as the old, but pulls in control files (NG_ROOT/lib/*.rota) 
#		to determine what work to do.
#		
# Date:		26 Dec 2004
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
if [[ ! -f $STDFUN ]]
then
	echo "cannot find standard functions"
	exit 1
fi
. $STDFUN

# --------------------------------------------- functions ------------------------------------

# prune the log files listed.  Keeps $1 lines; assumes files are in $NG_ROOT/site/log if not qualified
# intended to prune log files created by things run here; assumes they are not running
# invoked by: prune_log [day-name] nkeep file [file...]
function prune_log
{
	typeset keep=""
	typeset l=""
	typeset need=""

	case $1 in 
		[0-9]*)		keep=$1; shift;;

		*)		
				need=$1;				# if not numeric, assume its day of week test
				keep=$2;
				shift 2
				if [[ $today != $need ]]
				then
					log_msg "prune_log: dow mismatch ($today ! $need) log(s) not pruned: $*"
					return;
				fi
				verbose "prune_log: dow match ($today) log(s) will be pruned: $*"
				;;
	esac
	
	for l in "$@"
	do
		eval l=$l			# dereference if user coded $TMP or somesuch 
		if [[ $l != */* ]]			# allow logs outside of $NG_ROOT/site/log
		then
			l=$NG_ROOT/site/log/$l
		fi
		verbose "prune_log: attempt to prune: $l"
		if [[ -f $l ]]
		then
			if tail -$keep $l >$l-
			then
				mv $l- $l
				log_msg "prune_log: log pruned to $keep lines: $l"
			else
				log_msg "prune_log: log pruned failed: $l"
			fi
		else
			log_msg "prune_log: cannot find file to prune: $l"
		fi
	done
}

if ! whence ksh >/dev/null 2>&1			# if path is buggered in cartulary try to get something that will work
then
	if [[ -d $NG_ROOT/lib/cartulary.min ]]
	then
		. $NG_ROOT/lib/cartulary.min
	fi
	if [[ -d $NG_ROOT/site/cartulary.min ]]
	then
		. $NG_ROOT/site/cartulary.min
	fi
fi


# -------------------------------------------------------------------
ustring="[-f table-file] [-n] [-q] [-t section] [-v] [-V]"
ng_dt -c | read today junk	# get day of week
ctls=""				# list of control files we will parse
what=""				# section(s) from control files we will execute
forreal=1			# off when -n is set to only list what we might do
test=0				# set when -t to test a section
prune=0;			# set to 1 at midnight to prune OUR logs (not log_prune cmds in file)
version="ng_rota v3.2/0a187"
verbose=1
force=0				# run even if safe2run says no (no green-light)
debug=0				# extra verbose messages from the control file parser
export myhost=$(ng_sysname)

prunef="$TMP/rota.prune.$$" 	# list of log files to prune if its time

while [[ $1 == -* ]]
do
	case $1 in 
		-f)	ctls="$ctls$2 "; shift;;		# allow testing from a specific one -- do not pull in the ones in lib
		-F)	force=1;;				# allow manual execution even if green-light is not on.
		-n)	forreal=0;;				# just say what would have been done
		-t)	test=1; what="$what $2"; shift;;		# allow testing of a section

		-q)	verbose=0;;
		-v)	verbose=1;;
		-V)	debug=1; verbose=1;;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

cd $NG_ROOT/site	# if something core dumps, do it here

if [[ -z $ctls ]]
then
	(
                ls $NG_ROOT/apps/*/lib/*.rota $NG_ROOT/apps/*/lib/rota.* 2>/dev/null        # separate commands force preference order
                ls $NG_ROOT/site/*.rota $NG_ROOT/site/rota.* 2>/dev/null        
                ls $NG_ROOT/lib/*.rota $NG_ROOT/lib/rota.* 2>/dev/null
        ) | awk '
	/\/apps\// {				# allow apps to have duplicate names between them, but not dupd with site/lib
                x = split( $1, a, "/" );

                base = a[x];

		if( base == "main.rota" || base == "rota.main" || base == "build.rota" )	# application cannot have one of these!
		{
                        ignored = ignored $1 " ";
			next;
		}
		
                seen[base]++;			# keeps same named table in lib/site from being used
                list = list $1 " ";
		next;
	}

        {
                x = split( $1, a, "/" );
                base = a[x];
                if( ! seen[base]++ )            # if this unique name not seen
                        list = list $1 " ";
                else
                        ignored = ignored $1 " ";
        }
        END {
                printf( "%s:%s\n", list, ignored );
        }
        '| IFS=: read ctls ignored
fi

log_msg "$version started: cluster=$NG_CLUSTER" 

if ng_goaway -c $argv0
then
        ng_log $PRI_INFO $argv0 "not executing, found goaway file"
        log_msg "not executing; found goaway file"
        cleanup 0
fi

if ! safe2run
then
	if (( $force < 1 ))
	then
		msg="not executing, no green light"
		ng_log $PRI_INFO $argv0 "$msg"
		log_msg "$msg"
		cleanup 0
	fi

	log_msg "green light is OFF; executing anyway because of -F option!"
fi
		

date "+%H %M %a %d" | read hr min dow dom
ng_get_nattr | read nattrs

if [[ -z $what ]]				# not supplied from command line
then
	what=everytime 				# by default

	case $min in				# things run multiple times an hour
		0|00)	what="$what hourly quarter half";;
		30)	what="$what quarter half thirty";;
		15|45) 	what="$what quarter"	;;
		10)	what="$what tenpast";;		# an hourly, but not at the top of the hour
	esac
	

	if (( $min >= 0 && $min < 5 ))		# daily things go at the top of the hour, but allow for some fudge in the time; exec if run 0-5 min
	then
		if (( $hr % 2 == 0 ))
		then
			what="$what even_hr"
		else
			what="$what odd_hr"
		fi

		case $hr in
			00|0)	prune=1; what="$what midnight";;
			01|1)	what="$what fourhourly threehourly sixhourly";;
			03|3)	what="$what earlyam";;
			04|4)	what="$what threehourly build";;
			05|5)	what="$what fourhourly predawn";;
			06|6)	what="$what twice_daily";;
			07|7)	what="$what threehourly sixhourly";;
			09|9)	what="$what fourhourly";;
			10)	what="$what threehourly latemorn";;
			12)	what="$what noon";;
			13)	what="$what fourhourly threehourly sixhourly";;
			16)	what="$what threehourly afternoon";;
			17)	what="$what fourhourly";;
			18)	what="$what twice_daily";;
			19)	what="$what threehourly sixhourly";;
			20)	what="$what evening";;
			21)	what="$what fourhourly";;
			22)	what="$what threehourly night";;
		esac
	fi
else
	verbose "testing with these catigories: $what"
fi

verbose "control files ignored: $ignored"
verbose "processing using these control files: $ctls"
verbose "running commands associated with these catigories: $what"
verbose "parsing control files begins..."

# from each control file, extract the lines from the section(s) that were 
# given on the command line (what).  Further prune based on the evaluation
# of ifset, ifattr, ifdow, ifdom, and ifnoqueue 'commands'.  Output of this awk
# is subshell fragments enclosed in parens that are executed. The output 
# of each fragment is directed to its own log file based on the section 
# from which it was pulled. 
#
for f in $ctls
do
	echo "ctl: $f"
	cat $f
done | sed 's/#.*$//; s/^[ 	]/exec: /' | awk  \
	-v test=$test \
	-v log_base="$NG_ROOT/site/log/rota" \
	-v what="$what " \
	-v prunef="$prunef" \
	-v dow="$dow" \
	-v dom="$dom" \
	-v nattr_list="$nattrs" \
	-v verbose=$verbose \
	-v debug=$debug \
	'
	function set_redirect( )		# set the redirect for a fragment based on the section label
	{
		if( test )
			redirect = "";
		else
			redirect = sprintf( " >>%s.%s 2>&1", log_base, lab );
	}

	function isdow( need_list,		i, n, a, x, y )		# return true if current day of week matches need
	{
		n = split( need_list, a, "," );			# allow mon or mon,wed,fri

		for( i = 1; i <= n; i++ )
		{
			need = a[i];
		
						# allow upper/lower, and M, Mo, Mon, Monday etc.
			x = substr( need, 1, 1 );
			y = substr( need, 2, 1 );
			if( x == "M" || x == "m" )
				need = "Mon";
			else
			if( x == "T" || x == "t" )
			{
				if( y == "h" || y == "H" )
					need = "Thu";
				else
					need = "Tue";
			}
			else
			if( x == "W" || x == "w" )
				need = "Wed";
			else
			if( x == "F" || x == "f" )
				need = "Fri";
			else
			if( x == "S" || x == "s" )
			{
				if( y == "u" )
					need = "Sun";
				else
					need = "Sat";
			}
	
			if( need == dow )
				return 1;
		}
	
		return 0;
	}

	function isdom( need, 		i, a, x )	# return true if need is today, need may be 1,15,30
	{
		x = split( need, a, "," );
		for( i = 0; i <= x; i++ )
		{
			if( a[i] == dom )
				return 1;
		}

		return 0;
	}

	
	function cmd( c, 	i, res, buf )		# execute the command c and return the first line of output
	{
		res = "";
		while( (c|getline buf)> 0 )
		{
			if( !res )
				res = buf;
		}
		close( c );

		return res;
	}

	function isattr( need )				# return true if the attribute need is set for the node, need may be ! 
	{
		if( substr( need, 1, 1 ) == "!" )
			return ! has_nattr[substr( need, 2 )];
		else
			return has_nattr[need];
	}

	function isset( need, 	val )
	{
		val = cmd( "ng_ppget " need );
		if( val != "" )
			return 1;
		return 0;
	}

	function wstate( resource, 	cmd )		# return true if the resource has no queued or running jobs in woomera
	{
		if( ! wqueue[resource] )
		{
			ans = cmd( "ng_wstate -r -q " resource );
			if( ans == "0 0" )
				wqueue[resource] = 0;
			else
				wqueue[resource] = 1;	# dont care how many, just that it is not zero
		}

		return ! wqueue[resource];
	}

	BEGIN {
		x = split( nattr_list, a, " " );
		for( i = 1; i <= x; i ++ )
			has_nattr[a[i]]++;			# quick index based on nattr string
	}

	debug > 0 { printf( "DEBUG: (%s)\n", $0 ) >"/dev/fd/2"; }

	NF < 1 { next; }					# skip blank lines, up-front sed removed comments

	$1 == "ctl:" { fname = $2; next; }

	$1 == "exec:" {						# a line that was indented with a tab
		if( NF > 1 && snarf )				# it has size, and is in a section that we want
		{
			if( $2 == "prune_log" )
			{
				printf( "%s %s %s\n", $3, $4, $5 ) >prunef;		# can be: [dow] nlines logname
				next;
			}

					# parse the line looking for if*, sync, or set tokens; stop when we dont recognise next token
			async = "&";
			i = 2;			# skip over the exec: first token
			again=1			# until we find a token we dont know
			while( again )
			{
				again = 0;	# assume we dont recognise it
				parm = $(i+1);

				if( $(i) == "ifset" )			# if the pinkpages var is set
				{
					if( isset( parm ) )
					{
						if( verbose )
							printf( "pinkpages var is set (%s) continue parse of: %s\n", parm, $0 ) >"/dev/fd/2";
					}
					else
					{
						if( verbose )
							printf( "NOT EXECUTED pp var is not set (%s): %s\n", parm, $0 ) >"/dev/fd/2";
						next;
					}
					i += 2;
					again = 1;
				}
				else
				if( $(i) == "ifattr" ) 
				{
					if( isattr( parm ) )
					{
						if( verbose )
							printf( "attribute good (%s) continue parse of: %s\n", parm, $0 ) >"/dev/fd/2";
					}
					else
					{
						if( verbose )
							printf( "NOT EXECUTED attribute mismatch (%s): %s\n", parm, $0 ) >"/dev/fd/2";
						next;
					}
					i += 2;
					again = 1;
				}
				else
				if( $(i) == "ifdow" ) 
				{
					if( isdow( parm ) )
					{
						if( verbose )
							printf( "dow good (%s) continue parse of: %s\n", parm, $0 ) >"/dev/fd/2";
					}
					else
					{
						if( verbose )
							printf( "NOT EXECUTED dow mismatch (%s): %s\n", parm, $0 ) >"/dev/fd/2";
						next;
					}
					i += 2;
					again = 1;
				}
				else
				if( $(i) == "ifdom" ) 
				{
					if( isdom(  parm ) )
					{
						if( verbose )
							printf( "dom good (%s) continue parse of: %s\n", parm, $0 ) >"/dev/fd/2";
					}
					else
					{
						if( verbose )
							printf( "NOT EXECUTED dom mismatch (%s): %s\n", parm, $0 ) >"/dev/fd/2";
						next;
					}
					i += 2;
					again = 1;
				}
				else
				if( $(i) == "ifnoqueue" ) 
				{
					if( wstate( parm ) )
					{
						if( verbose )
							printf( "nothing queued (%s) continue parse of: %s\n", parm, $0 ) >"/dev/fd/2";
					}
					else
					{
						if( verbose )
							printf( "NOT EXECUTED queue not empty (%s): %s\n", parm, $0 ) >"/dev/fd/2";
						next;
					}
					i += 2;
					again = 1;
				}
				else
				if( $(i) == "sync" )		# causes the command to be run synchrounusly
				{
					i++;
					async="";
					again = 1;
				}
				else
				if( $(i) == "set" )		# allows for set -x set +x
				{
					i++;
					async="";
					again = 1;
				}
			
			}
			
			if( $NF == "&" )			# if user added it, we dont
				async="";		


			for( j = 1; j < i; j++ )
				$(j) = "";			# whack exec: and if* tokens; must do here because of SGI awk madness

			if( match( $0, "[a-z[A-Z]" ) )			# if we ended up with something real
			{
				if( ! out_lab++ )			# first from this section, put out starting stuff
				{
					printf( "\n# from section: %s:%s\n", fname, lab );		
					printf( "log_msg \"starting section %s\"\n", lab );	# goes to main rota log file
					printf( "(\n" );					# cause cmds to be run in subshell
					printf( "\tlog_msg \"starting\"\n", lab );		# goes to rota.label log file
				}
				gsub( "^[ 	]*", "	", $0 );				# knock off any leading whitespace
				printf( "%s %s\n", $0, async );					# dump the line and add & if needed
			}

		}
		else
			if( debug )
				printf( "DEBUG: ignored: snarf=%d (%s)\n", snarf, $0 ) >"/dev/fd/2";
		next;
	}						# end exec:

	{					# assume section label
		if( out_lab )			# close previous fragment if it was started
			printf( "\twait\t# wait for all tasks in this subshell\n) %s &\n", redirect );

		out_lab = 0;

		if( index( " " what " " , " " $1 " " ) )	# if it was in the what to do list
		{
			snarf = 1;		
			lab = $1;		# section label used in comment, log msgs and output log filename
			set_redirect( );	# compute the output log file name for the fragment
			printf( "parsing section: %s redirecion to %s\n", lab, redirect ) >"/dev/fd/2";
		}
		else
			snarf = 0;
		if( debug )
			printf( "DEBUG: newsection: snarf=%d (%s)\n", snarf, $0 ) >"/dev/fd/2";
	}

	END {
		if( out_lab )			# close the last section if it was left pending
			printf( " wait\t# wait for all tasks in this subshell\n) %s &\n", redirect );
	}
	
' >$TMP/rota_tasks.$$
rc=$?

verbose "parsing control file(s) ends ($rc)"

if (( $rc != 0 ))
then
	log_msg "abort: control file parse failed."
	cleanup 1
fi

if (( $forreal > 0 ))
then
	if (( $test < 1 ))
	then
		verbose "building cartulary before doing anything else"
		$NG_ROOT/bin/ksh $NG_ROOT/bin/ng_pp_build -v >>$NG_ROOT/site/log/pp_build 2>&1	# rebuild the cartulary MUST be first, and synch
	fi

	# pruning is done first and in sync so that logs generated by commands in the rota file can be safely pruned before execution
	if (( $prune > 0 ))			# time to prune our own logs?
	then
		verbose "pruning various log files"
		#for x in midnight earlyam predawn morning noon afternoon evening night everytime hour quarter half
		for x in $(ls $NG_ROOT/site/log/rota.*)		# get any rota.foo log
		do
			prune_log 5000 ${x##*/}
		done

		prune_log 5000 pp_build
	fi

	if [[ -s $prunef ]]			# log files that were marked with prune_log in a section
	then
		while read parms
		do
			if [[ -n $parms ]]
			then
				prune_log $parms
			fi
		done <$prunef
	fi

	if [[ -s $TMP/rota_tasks.$$ ]]
	then
		verbose "starting task execution"
		if (( $verbose > 0 ))
		then
			cat $TMP/rota_tasks.$$
		fi

		. $TMP/rota_tasks.$$
		verbose "all tasks kicked off; waiting"
		wait
		verbose "exiting now"
	else
		log_msg "command list was empty; nothing run"
	fi

else
	verbose "no execute mode set; would have executed:"
	cat $TMP/rota_tasks.$$
fi

cleanup 0


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_rota:Drive periodic roll call of Ningaui tasks)

&space
&synop	ng_rota [-f table-file] [-F] [-n] [-q] [-t table-section] [-v] [-V]

&space
&desc	&This is invoked every few minutes from &lit(cron) to drive
	one or more functions that need to be executed on a regular
	basis.  This script is used, instead of driving the programmes
	directly from &lit(cron)  as it ensures that the Ningaui environment
	is established properly, allows for conditional execution of commands
	based on node attribute and current ng_woomera queue states,
	and does not require the &lit(crontab)
	for the Ningaui user ID to be modified when new tools must be 
	introduced. 
	
&space
	Commands are executed via rota tables installed in 
	&lit(NG_ROOT/apps/*/lib,) &lit(NG_ROOT/lib) and &lit(NG_ROOT/site).
	&This will use all tables matching the filename patterns &lit(rota.*) or &lit(*.rota)
	which exist in any of the three previously mentioned directory paths. 
	Duplicate filenames are permitted between apps directories, but if the same name 
	(e.g. build.rota) exists in an apps directory and in the NG_ROOT/site or NG_ROOT/lib
	directory, the copy in the NG_ROOT directory will be ignored. 
	The files &lit(main*.rota) or &lit(rota.main*) are not permitted in the apps
	directories; they will be ignored if found.  
	It is strongly recommended that all tables in the apps directories use a unique
	application mnemonic as the leading part of the table filename (e.g. aether_main.rota). 
&space	
	The tables are divided into multiple sections, each section containing zero or 
	more commands.  The commands in each section are executed if it is the 
	proper time for the section when &this is started. The following are the 
	section names and the time(s) that they are executed:
&space
&indent
&begterms
&term	everytime : These commands are executed every time &this is run.
&space
&term	even_hr : This target causes the commands listed to be executed at the top of each even numbered hour.
&space
&term	odd_hr : This target causes the commands listed to be executed at the top of each odd numbered hour. 
&space
&term	twice_daily : Commands listed on this target are executed at 6am and 6pm.
&space
&term	sixhourly : The commands listed for this tag are executed every six
	hours (1am, 7am, 1pm, and 7pm).
&space
&term	fourhourly : Commands listed for this target are executed every fourth hour beginning at 1am.
&space
&term	threehourly : Commands listed for this tag are executed every three
	hours begiingin at 1am.
&space
&term	hourly : At the top of each hour: xx:00.
&space
&term	tenpast : Hourly, but at 10 past each hour.  Helps to avoid collisions with things that need to 
	run hourly and things that run daily. 
&space
&term	half : On the hour and on the half hour: xx:00, and xx:30.
&space
&term	quarter : Every 15 minutes:  xx:00, xx:15, xx:30, and xx:45.
&space
&term	midnight :	About midnight each night.
&space
&term	earlyam :	Approximately 3:00 each morning.
&space
&term	build : 	Reserved for package builds; about 4:00am.
&space
&term	predawn :	About 5:00 each morning.
&space
&term	morning :	Just before noon, about 10:00am each morning.
&space
&term	noon :		At noon.
&space
&term	afternoon :	About 16:00 each afternoon.
&space
&term	evening :	These are executed about 20:00 each evening.
&space
&term	night :		Just before midnight; approximately 22:00. 
&space
&endterms
&uindent
&space

&subcat	Special Sections 
	Special sections, named using any name that is not listed above, can be added to a rota table for testing.  
	The -t option is  used to execute (test) sections that will not be executed automatically.
	The -t option can also be used to execute a standard named section manually. 

&subcat	Additional Tables
	&This will read all available tables, and execute the appropriate section(s)
	from each of the tables parsed.  This allows node specific jobs to be placed 
	into a table that will not be overlaid with each install. The order that tables
	and commands are processed is NOT guarenteed.  Causing &this to schedule a command
	via &lit(ng_woomera) is one way that ordering can be addressed.  An example of this 
	is presented later when avoiding concurrent job execution is discussed. 

&space	
&subcat Conditional Execution
	A command can be restricted to run on systems based on one of several criteria:
	wether or not a pinkpages variable is set, 
	a node attribute, 
	the day of the week, 
	the day of the month, 
	or
	the state of a ng_woomera resource.  

	These conditions are implemneted using the followng keywords, each of which is followed
	by a value, that when true allows the command to execute.

&space
&begterms
&term	ifattr atrribute : The command is executed if the node currently has the indicated 
	atribute.  For example, &lit(ifattr Jobber) will allow the command to execute only 
	on the node that has the Jobber attribute (see ng_get_nattr). The not symbol (!) 
	can be placed infront of the attribute name (no whitespace) to cause the command 
	to be executed on a node that does not have the indicated attribute (e.g. !Sattelite).
&space
&term	ifdow day-name : Will allow the command to execute only if the current day of the 
	week matches &ital(day-name).  Day names may be upper or lower case, and are the 
	accepted day of week name abbreviations returned by the date command.  If a command 
	is allowed to execute on multiple days of the week, the names can be separated
	by commas rather than requiriing muliple entries of the same command.
&space
&term	ifdom date : Similar to the ifdow token, this allows the command to execute only when 
	the day of the month matches &ital(date.)  &ital(Date) may also be a comma separated
	list (e.g. 1,5,10). 
&space
&term	ifnoqueue resource : A check will be made against woomera's queues and if there are 
	no jobs (running or queued) which reference the named resource, the command will be 
	executed.  More than one  resource can be supplied if space separated and enclosed in 
	double quotes.
&space
&term	ifset ppvarname : If the named pinkpages variable is set (has any non-empty value) then 
	the statement will be allowed to execute.  
&endterms
&space
	The conditional execution keyword value pairs may be 'stacked' allowing, for example, 
	a command to be executed on Mondays if the node is the Steward:
&space
	ifattr Steward ifdow Mon ng_somecommand
&space


&subcat	Previously Executed Commands
	Commands are not checked to see if they are still running from the last time they 
	were started by &this.  If it is possible for a command to run longer than the 
	period of time between segment invocations, then the command should be executed 
	via &ital(woomera) and given a resource with a limit of one (1).  This allows 
	&this to start another command as is expected, but will prevent it from concurrently 
	executing with another.  The following is an example of the rota table entry that 
	can be used to do this:
&space
&litblkb
  sync ng_wreq limit Rota_flist_send 1
  ng_wreq job : Rota_flist_send cmd ng_flist_send
&litblke
&space

	Note that the command that sets the limit is preceded with a &lit(sync) keyword. 
	Normally &this invokes the commands in the list sequentially, but allows them all
	to execute concurrently.  The &lit(sync) keyword forces &this to execute and wait 
	for the command to finish before invoking other commands from the list. This allows
	the limit to be set before the ng_woomera job is started.  
&space
	An alternate method of invoking the above job uses the &lit(ifnoqueue) conditional
	to prevent multiple jobs from being queued in ng_womera:
&space
&space
&litblkb
  sync ng_wreq limit Rota_flist_send 1
  ifnoqueue Rotat_flist_send ng_wreq job : Rota_flist_send cmd ng_flist_send
&litblke
&space


&subcat	Log Files
	It is recommended that the standard output and standard error of each command be 
	redirected on the command line.  By default, &this will redirect standard output 
	and error to a logfile in &lit(NG_ROOT/site/log) using the basename of &lit(rota) 
	and a filename suffix that matches the section name (e.g. hourly).  Quoting of
	redirection symbols (>, >>, and <) is necessary when commands are submitted to 
	ng_woomera; if the redirection tokens are not quoted the stderr/out for the &lit(ng_wreq) 
	command will be captured in the file(s) and not the output of the command executed 
	by ng_woomera. The same holds true if using ng_wrapper to execute a command. 

&space
&subcat	Pruning Logs
	Log files created by commands executed by &this can easily be pruned with &lit(prune_log)
	statements in the control file.  
	The &lit(prune_log) statement has the syntax:
&space
&litblkb
   prune_log [day-abbr] nkeep file [file...]
&litblke
&space

	The optional &ital(day-abbr) is a day of the week name abbreviation (e.g. Fri). When specified,
	the log will be pruned only on Fridays. 
	The &ital(nkeep) parameter supplies the number of lines to keep in each of the named files. 
	&This ensures that all of the prune log file commands are executed before any of the 
	tasks are started.  
	The log pruning function assumes that the processes that create/update the log files are 
	not running; undesired results will occur if a log is pruned while open by another process. 


&space
&parms	These command line options are supported:
&begterms
&term	-f file : Supplies a single table file to use.  Table files in the library directory 
	are ignored.  Multiple -f options are allowed.
&space
&term	-F : Force.  If the standard function &lit(safe2run) returns a non-zero exit code, then
	&this will immediately exit. This prevents executing anything from rota if the node
	has not been initialised, or if the node is stopping.  This option allows &this to be
	executed manually and will override the safe2run check.
&space
&term	-n : No execute mode.  Would list what it would run, but will not really do anything.
&space
&term	-q : By default verbose mode is on; this option turns it off.
&space
&term	-t section : Table section to run.  Pretends like it is time for the section named 
	to be executed and runs those commands.  The named section will be executed &stress(from each)
	table that is encoutered (or supplied with -f).  No other sections will be processed
	even if the current time matches the normal execution time of the section. This is 
	useful for testing, or causing the re-execution of a sction (e.g. build).
&sapce
&term	-v : Verbose mode. By default, because rota is run with a lights out mentality, we 
	set verbose on.  This is a convenience option to be consistant with other commands 
	and not cause an error if supplied. 
&space
&term	-V : Debug mode.  Lots of debugging messages as the config file parsing occurs.
&endterms

&space
&see	ng_woomera, ng_get_nattr, ng_set_nattr, ng_green_light

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	23 Jul 2001 (sd) : Hot off of the press. 
&term	13 Mar 2002 (sd) : changed catalogue ckpt call to happen only from leader
&term	15 Mar 2002 (sd) : To drive host stats if running on a remote host
&term	06 Sep 2002 (ah) : support services other than leader
&term	15 Mar 2003 (sd) : added pp_build call
&term	01 Apr 2003 (sd) : pulled ccp_mon from hourly (deprecated by ccp_timeout).
&term	17 Apr 2003 (sd) : Added support to skip execution if goaway file is there.
&term	25 Jun 2003 (sd) : added mysql si database stuff.
&term	16 Jul 2003 (sd) : Moved pp_prune inside of node if.
&term	03 Aug 2003 (sd) : Added support for satellite nodes.
&term	03 Oct 2003 (dk) : Added documentation of Stats role.
&term	03 Dec 2003 (sd) : Added send of node attributes to seneschal on jobber node.
&term	16 Dec 2003 (sd) : Added ifattr to support node attribute things.
&term	28 Jan 2003 (sd) : Removed iftype node things; all nodes are nodes now.
&term	05 Mar 2003 (sd) : Added dawn/evening/night support.
&term	19 Mar 2003 (sd) : Corrected set catalogue cluster if statement.
&term	15 Jul 2004 (ah) : Added waldo things.
&term	17 Aug 2004 (sd) : Now logs cluster on start message.
&term	18 Aug 2004 (sd) : Added log prune for log files generated by rota jobs.
&term   18 Aug 2004 (sd) : Converted srv_Leader references to corect srv_ things.
&term	30 Sep 2004 (sd) : Added tmp clean of site/woomera. (HBD KDG)
&term	19 Oct 2004 (sd) : removed ng_leader refs. 
&term	09 Nov 2004 (sd) : removed fetchftp invocation.
&term	06 Jan 2005 (sd) : V2 conversion -- now with table manners.
&term	13 Jan 2005 (sd) : Day of week may now be supplied as all lowercase, or full name.
			Added ability to prune logs on specific day of week (prune_log dow n name)
&term	19 Jan 2005 (sd) : Corrected section name lookup match error.
&term	27 Jan 2005 (sd) : Corrected issue with missing my host.
&term	03 Mar 2005 (sd) : Added check which causes an immediate exit if safe2run returns bad.
&term	01 Jun 2005 (sd) : Added build tag for 4am.
&term	04 Jul 2005 (sd) : Fixed typo in prune if statement -- now prunes rota's logs at midnight.
&term	08 Feb 2005 (sd) : Added dom function to allow things to run on a specific day of the month, 
		and allowed day of week to be supplied as a comma separated list.
		Revampped the man page. 
&term	23 May 2006 (sd) : Added ifnoqueue support.
&term	30 May 2006 (sd) : Added synch checking for noqueue, and support for sync keyword. 
&term	23 Jun 2006 (sd) : Added verbose option to pp-build call.
&term	24 Jul 2006 (sd) : Enhanced interpretation in ifXXX conditional processing to make quoting 
		easier.  Using ng_wrapper with -o1 and -o2 options is still the only recommended 
		way to kick a job off with output redirection.
&term	05 Aug 2006 (sd) : Added better control over output of debug messages (-V).
&term	23 Aug 2006 (sd) : Completely reworked the parsing of the control file so as to allow 
		as many if* commands without any quoting issues. Quotes round redirection are needed
		only for things like wrapper or wreq commands. 
&term	19 Sep 2006 (sd) : Now allows for control files in site (overrides lib).
&term	01 Dec 2006 (sd) : Added the sixhourly label to kick something off every six hours: 1am, 7am, 1pm and 7pm.
&term	01 Feb 2007 (sd) : Added the threehourly label to kick something off every thres hours starting at 1am.
		Added the tenpast label to help with log_frag and log_comb collisions.
&term	07 Feb 2007 (sd) : Added small diagnostic to verbose output to identify the file that the section of 
		commands was pulled from.
&term	30 Mar 2007 (sd) : Updated man page to drop reference to Logger.
&term	08 May 2007 (sd) : Added search of $NG_ROOT/apps/*/lib for control files.
&term	26 Jun 2007 (sd) : Fixed typo -- looked like a misplaced line. 
&term	29 Jun 2007 (sd) : Script now changes working directory to NG_ROOT/site so we dont leave droppings
		in ~ningaui.
&term	01 Oct 2007 (sd) : Changed the way we lop off tokens when processing if* conditionals. On the sgi, 
		setting $(n)="" annoyingly shifts the other tokens over one. 
&term	19 Oct 2007 (sd) : Fixed problem with dow in prune_log call.  Added documentaion on the &lit(prune_log)
		function to the man page. 
&term	19 Feb 2008 (sd) : Added a check to catch awk failure during control file parse. 
&term	04 Sep 2008 (sd) : Added ifset conditional to allow conditional execution only if a pinkpages variable is 
		set (not empty) for the node. 
&term	16 Oct 2008 (sd) : Added an explicit reference to NG_ROOT/bin/ksh on the pp_build call so that if the 
		cartulary is hosed we can make every effort to get it rebuilt. 
&term	11 Nov 2008 (sd) : Prune_log function now dereferences the log name(s) so that $TMP or other vars can be used.
&term	17 Jan 2009 (sd) : Added twice_daily, even_hr, odd_hr, and fourhourly target support.
&term	06 May 2009 (sd) : Corrected a typo in the twice daily keyword assignment that was preventing twice daily
		actions from being executed.
&endterms

&scd_end
------------------------------------------------------------------ */

