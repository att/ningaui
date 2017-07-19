#!/usr/bin/env ksh
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


#!/usr/bin/env ksh
# Mnemonic:	yapp - yet another process printer
# Abstract:	Accepts one or more process ids or a string and generates a 
#		listing of processes in a parent/child tree format. If no 
#		parms are entred then all processs are listed. If a string 
#		is given then all processes listed by ps that have the string
#		are formatted and printed.  By default, a processes whole 
#		lineage (from the init process through the requested process
#		to its last child/grandchild) is displayed. The -s option 
#		causes the liniage to start with the process rather than 
#		with the init process. If more than one pid is given or a 
#		string matches more than one process, then a listing for 
#		each lineage is given.  If more than one of the process ids
#		exists in the same lineage, the lineage is listed only once. 
#		Deigned for a BSD host, but given hooks that should allow it 
#		to run on a System V (or even A!X) host.
# Date:		04 January 2003
# Author: 	E. Scott Daniels
# ---------------------------------------------------------------------------
# 
#

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# andrew's old pstree script -- incorporated
function andrew_pstree
{
	ng_ps | gre -v $$| awk -v target=$1 '
		BEGIN	{ 
			if( target+0 <= 0 )
			{
				printf( "invalid process id: %s\n", target );
				exit( 1 );
			}
			go = 1;
		}
		{
			x = $0; sub(".*:...", "", x); cmd[$2] = x
			parent[$2] = $3
		}
	function par(x, s, indent, goo){
		if(parent[x]+0 > 0) indent = par(parent[x], " ")
		else indent = ""
		goo = indent
		if(s != " "){
			goo = indent
			gsub(".", s, goo)
		}
		print goo x " " cmd[x]
		return indent "    "
	}
	function sib(x, indent){
		for(i in parent) if(parent[i] == x){
			print indent i " " cmd[i]
			sib(i, indent "    ")
		}
	}
	END	{
			if( go )
			{
				x = par(target, ">")
				sib(target, x)
			}
		}'
}

function usage
{
	echo "usage: $argv0 [-h] [-s] [-t trunc] [-u user] [-w] [--sysv]"
	echo "       $argv0 -H pid"
	cleanup 1
}
# --------------------------------------------------------------------------------------

all=1		# show all processes in a chain; -s turns to short listing showing just from the requested name down
trunc=180	# point at which the command information is truncated (chrs)
html=0		# generates HTML output if set (-h)

usr=""
what=""

# ng_ps offsets
uid=1
pid=2
time=5
cmd=7

while [[ $1 = -* ]]
do
	case $1 in 
		-h)	html=1;;
		-H)	shift; andrew_pstree "$@"; exit;;
		-s)	all=0;;
		-t)	trunc=$2; shift;;
		-u)	what=$2; usr=$2; shift;;
		-w)	wide=1;;


		-v)	verbose=1;;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;



	esac

	shift
done

if [[ -z $what ]]
then
	what=${1:-1}
fi

ng_ps >/tmp/yapp.$$
cp /tmp/yapp.$$ /tmp/daniels.xx
if [[ $what = [a-zA-Z_]* ]]
then
	grep $what /tmp/yapp.$$ | gre -v "yapp|grep" |awk -v pid=$pid ' { p = p " " $(pid); } END {print p}' |read what
fi

awk \
	-v usr="$usr" \
	-v what="$what" \
	-v all=$all \
	-v trunc=$trunc \
	-v bsd=$bsd \
	-v linux=$linux \
	-v wide=$wide \
	-v html=$html \
	-v uid=$uid \
	-v pid=$pid \
	-v time=$time \
	-v start=$time \
	'

	# return row colour for html, or null if not in html mode
	function bgcolour( )
	{
		if( html )
		{
			if( ++bgc_out > 2 )
			{
				bgc_idx = (++bgc_idx) % 2;
				bgc_out = 0;
			}
			return bgcolourv[bgc_idx];
		}

		return "";
	}

	function print_chain( sid, tab, rsibs, 		blank, a, i, x )
	{
		printed[sid]++;

		blank = 0;
		if( rsibs )			# siblings after this?
			mytab = tab;		# yep, just use tab string for our info
		else
			mytab = substr( tab, 1, length( tab ) -1 ) "|";	# nope, replace last blank with bar

		stuff = sprintf( "%s- %-6d", mytab, sid );
		printf( fmt "\n", bgcolour( ), stuff, substr( cmd[sid], 1, trunc ) );

		if( children[sid] )
		{
			x = split( children[sid], a, " " );	
			for( i = 1; i <= x; i++ )
			{
				if( blank )
				{
					if( html )
						printf( "<tr bgcolor=%s><td><pre>%s</pre></td><td><br></td></tr>\n", bgcolour( ), tab (x >= 1 ? "  |" : "   ") );
					else
						printf( "%s\n", tab (x >= 1 ? "  |" : "   ") );
				}

				mytab = i < x ? "  |" : "   ";
				blank = print_chain( a[i], tab mytab, x - i );

			}
			blank++;		# if we printed children then caller needs to blank
		}

		return blank;
	}

	BEGIN {
		init_pid = 1;			# for all sane flavours init is still pid 1 
		cmd[1] = "root init";		# init does not always show in ps

		bgcolourv[0] = "#82f47b";
		bgcolourv[1] = "#ffffff";
		bgc_out = 99;

		if( html )
		{
			printf( "<table cellspacing=0 cellpadding=0 vspacing=0 >\n" );
			fmt = sprintf( "<tr bgcolor=%%s><td><pre>%%-%ds  </pre></td><td><pre>%%s</pre></td></tr>", wide ? 50 : 30 );		# format statement with tab field size

			printf( "<html><body text=\"black\" bgcolor=black><tr><td><br></td></td><td><br></td></td>\n" );
		}
		else
			fmt = sprintf( "%%s%%-%ds  %%s", wide ? 50 : 30 );		# format statement with tab field size
	}

	/COMMAND/ { next; }			# skip header

	/root.*zsched/ {
		if( $2 == $3 )			# another in the long list of bad sun things...
		{
			init_pid = $2;		# bloody sun breaks tradition where everything chans back to init == pid == 1
		}
	}

	{
		this = $(pid);		# snag process ids
		mom = $(pid+1);

		children[mom] = children[mom] " " this;		# we could get fancy and sort this list

		parent[this] = mom;	
		
		cmd[this] = sprintf( "%-12s ", $(uid) );
		user[this] = $(uid);

		for( i = start; i <= NF; i++ )
			cmd[this] = cmd[this] $(i) " ";	# likely more than command stuff
	}

	END {
		need_sep = 0;
		x = split( what, a, " " );

		for( i = 1; i <= x; i++ )
		{
			pid = a[i];
			if( !usr || user[pid] == usr )		# if user supplied, print only those trees owned by 
			{
				if( pid > 1 )
				{
					if( ! printed[pid] && cmd[pid] )
					{
						need_sep = 1;

						if( all )
						{
							while( parent[pid]+0 != init_pid )
								pid = parent[pid];
							if( html )
								printf( fmt "\n", bgcolour( ), "  1", substr( cmd[init_pid], 1, trunc ) );
							else
								printf( fmt "\n", bgcolour( ),  init_pid, substr( cmd[init_pid], 1, trunc ) );
						}
		
						print_chain( pid, "   ", 0 );	
					}
				}
				#else
				#	print_chain( pid, "   ", 0 );		# not allowed on suns as it generates garbage
	
				if( i < x  && need_sep )				# seperator
				{
					need_sep = 0;
					if( html )
						printf( "<tr bgcolor=\"%s\"><td><br></td></td><td><br></td></td>\n", bgcolour( ) );
					else
						printf( "\n" );
				}
			}
		}

		if( html )
			printf( "</table></body></html>\n" );
	}
' </tmp/yapp.$$

cleanup 0




/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_yapp:Yet another PS Printer)

&space
&synop ng_yapp [-h] [-s] [-t trunc] [-u user] [-w] [sring|pid]
&synop ng_yapp -H pid

&space
&desc	&This gets a current process listing via the ps command (ng_ps) and 
	formats the output in a tree like arrangement on standard output. An optional
	string or process-id may be supplied as the only positional parameter. If this 
	is supplied, the process 'chain' that contains the string or the process id is 
	the only chain printed.  If multiple occurances of &ital(string) are found in 
	the process listing, then all of their chains are printed.  

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-h : Generate HTML output -- convenient for running from a cgi-bin programme.
&space
&term	-H pid : Run the 'hume' version of yapp (old pstree). Pid is the process to suss
	out and generate information for. 
&space
&term	-s : Short listing.  If a &ital(string) or process id is given, the process chain 
	from the name/id is generated; the portion of the tree above the process is omitted. 
&space
&term	-t n : truncates the width of the output at n characters.
&space
&term	-u name : Display only processes associated with the named user.
&space
&term	-w : Display is set to wide mode.
&endterms


&space
&parms	&This accepts one positional parameter on the command line which may be either a
	&ital(string) or a &ital(process id).  When supplied, the process chain containing 
	the string or the process id is generated.  If omitted, all processes are displayed.

&space
&examp
	The command `ng_yapp woomera` will display all processes that have been started by 
	woomera and have not detached from its process group. 
&space
&see	ng_ps

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	04 Jan 2003 (sd) : Fresh from the oven.
&term	12 Oct 2006 (sd) : Added -H option.
&term	22 Oct 2006 (sd) : Added space between spacers (-) and pid.
&term	11 Nov 2008 (sd) : Added fix to work under new SunOS.  
&term	23 Jan 2009 (sd) : Corrected one more problem that was causing issues under the latest SunOS.
&endterms

&scd_end
------------------------------------------------------------------ */

