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
# Mnemonic:	ng_goad
# Abstract:	looks at the jobs in seneschal and creates a list of hint commands
#		to try to intice repmgr to move files in order to allow jobs to run
#		input to this script from seneschal is that of a dump 5.
#		
# Date:		13 Oct 2003
# Author:	E. Scott Daniels
# Bugs/irritations:
#		if multiple nodes are given with -a, and all files are on these nodes
#		goad has no idea as to where to hint what for the job; and does not complain.
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

PATH=$HOME/bin:$PATH

# pull all pending input files from the dump 5 file and send to repmgr to generate a 'join' file
function mkjoin
{
	verbose "sending dump1 requests to repmgr"
	awk '	/pending input:/ { printf( "dump1 %s\n",  $3 ); next; } ' <$fudgef | ng_rmreq -a |gre -v "^not found"
}

# -------------------------------------------------------------------------------------

forreal=1
jobber=$srv_Jobber
force=0				# will not force a move if at least one node can handle the job
mustonfile=""			# file with list of jobs and the nodes that they must run on
only="*"
ignore_goaway=0
verbose=0;
hint=1
hint_lease=$(( $( ng_dt -i ) + 864000 ))	# 24 hours on the hint-o-meter
state_only=0			# generate state, but not hint/hood commands to stdout
timeout=300

ustring="[-a avoid-node] [-F] [-f dump-5-file] [-g] [-h] [-n] [-m must-file] [-o only-consider-list] [-s jobbernode] [-t sec] [-v|V]"
while [[ $1 = -* ]]
do
	case $1 in 
		-a)	avoid="$2"; shift;;		# space separated list of nodes to avoid
		-F)	force=1;;
		-f)	fudgef=$2; shift;;		# dump 5 output
		-g)	ignore_goaway=1;;
		-h)	hint=0;;
		-n)	forreal=0;;
		-m)	mustonfile=$2; shift;;
		-o)	limiter=$2; shift;;	# only this 
		-s)	jobber=$2; shift;;
		-t)	timeout=$2; shift;;
		-v)	verbose=$(( $verbose + 1 ));;
		-V)	verbose=$(( $verbose + 1 )); state_only=1;;

		-\?)	usage; cleanup 1;;
		--man)	show_man; cleanup 1;;
		*)	echo "huh?"; usage; exit 1;;
	esac 

	shift
done

verbose "ng_goad version 2.0/0c226"

if ng_goaway -c ${argv0##*/} 				# allow to block easily if run from rota
then
	if [[ $ignore_goaway -lt 1 ]]
	then
		log_msg "not running, found go away file"
		cleanup 0
	else
		log_msg "goaway file found but ignored"
	fi
fi

if [[ -z $fudgef ]]
then
	verbose "requesting dump 5 from seneschal"
	fudgef=/tmp/fudgef.$$
	#ng_sreq -t $timeout -s $jobber dump 5 | grep "$limiter" >$fudgef
	echo "dump:5:" |ng_sendtcp -t 3500 -v -e "." $jobber:$NG_SENESCHAL_PORT >$fudgef

fi

(
	# list nodes that seneschal knows aobut and weed out those suspended, released, or without attributes so when selecting
	# a random node for a file (if we need to) we do not assign it to a dud.
	ng_sreq dump 30 | awk ' 
		/no-attr/ { next; }
		/SUSPENDED/ { next; }
		/RELEASED/ { next; }
		/^node:/  { split( $2, a, "(" ); printf( "member: %s\n", a[1]); next; }
	'
	cat $fudgef 
)| awk \
	-v hint=${hint:-0} \
	-v force=$force \
	-v avoid="${avoid:-nothing} " \
	-v verbose=$verbose \
	-v mustonfile="$mustonfile" \
	-v only_snarf="$limiter" \
	-v verbose=$verbose \
	'
	BEGIN {
		if( mustonfile )				# file with jobs that must run on a specific node
		{
			while( (getline<mustonfile) > 0 )
				must_on[$(NF-1)] = $NF;
			close( mustonfile );
		}

		members = " ";

		srand( );
	}

	function random_node( j, 	r, aa, x )
	{
		if( ok_nattr[j] )
			x = split( ok_nattr[j], aa, " " );
		else
			x = split( members, aa, " " );

if( x == 0 )
printf( "members=%s  nattr=%s\n", members, ok_nattr[j] )>"/dev/fd/2";
		r = int( ( (rand( ) * 1000) % x ) + 1 );	
#printf( "\n>>>j=%s x=%d r=%.0f n=%s (%s)\n", j, x, r, aa[r], ok_nattr[j] );
	
		return aa[r]; 
	}

	function do_input( )
	{
		file = $3;		# dump 5 formatted output order
		size = $4;
		nodes_start = 5		# field that node info starts in

		idx = jidx[job]+0;
		jidx[job]++;
		jneeds[job,idx] = file;
		fsize[file] = size;
		for( i = nodes_start; i <= NF; i++ )
		{
			if( !index( liveson[file], $(i) ) )
				liveson[file] = liveson[file] $(i) " ";
		}
	}

	# get a value in the current record of the form value(what)
	function getv( what,            i, j, k )
        {
                for( i = 1; i < NF+1; i++ )
                {
                        if( split( $(i), j, "(" ) == 2 )        # stuff(what)
                                if( what ")" == j[2] )
                                        return j[1];
                }

                return 0;
        }


	/member: /	{ members = members $2 " "; next }

	/needed input:/ { do_input( ); next; }
	/pending input:/ { do_input( ); next ; }	# new seneschal makes a difference 

	/pending nattrs:/ {				# job has attribute restrictions, snag nodes that are ok
		for( i = 1; i < NF && $(i) != "}" ; i++ );		# pending nattrs: { nattr1 natter2... }  node node node
		ok_nattr[job] = " ";
		for( i++; i <= NF; i++ )
			ok_nattr[job] = ok_nattr[job] $(i) " ";	# ensure first/last tokens have lead/trail space
		next;
	}

	/pending: /	{
		job = getv( "name" );
		if( (n = getv( "node" )) ) 			# this will fail for { Atribute Attribute }(node)
		{
			if( n != "<any>" )
			{
				gsub( "\\)", "", n );
				must_on[job] = must_on[job] n " ";
			}
		}

		ok_nattr[job] = " ";
		next;
	}

	END {
		for( job in jidx )
		{
			if( only_snarf &&  match( job, only_snarf ) <= 0  )		# -o pattern used to limit what we consider
			{
				if( verbose > 1 )
					printf( "did not consider job: %s\n", job ) >"/dev/fd/2";
				continue;
			}

			delete has;
			delete amt;			# size of all files on a node 
			jsize = 0;

			if( substr( must_on[job], 1, 1 ) == "!" )	# this is an avoid node now
				must_not_on = " "  substr( must_on[job], 2 );
			else
			{
				must_not_on = "";
				ok_attr[job] = must_on[job];			# this is the only allowed node for job; force via attribute 
			}

			for( i = 0; i < jidx[job]; i++ )
			{
				file = jneeds[job,i];
				jsize += fsize[file];
				n = split( liveson[file], a, " " );
				for( j = 1; j <= n; j++ )
				{
					has[a[j]]++;
					amt[a[j]] += fsize[file];		# target node with most amount of data
				}
			}

			printf( "%-30s needs %d files, %.0f bytes; ", job, jidx[job], jsize );
			stuck = 1
			ok_on = "";
			nok = 0;			# number ok
			most = 0;
			nmost = "";
			most2 = 0;
			nmost2 = "";
			for( h in has )
			{
				if( index( avoid must_not_on " ", h " " ) )   	# skip if h !in avoid list, or must not on list
					continue;

				if( ok_nattr[job] != " " )				# job has a list of nodes with ok attributes
					if( ! index( ok_nattr[job], " " h " " ) )	# skip it if  this is not on that list
						continue;

				if( has[h] == jidx[job] )
				{
					ok_on = sprintf( "%s %s", ok_on, h );
					stuck=0;
					nok++;
				}

				if( amt[h] > most )
				{	
					most2 = most;
					nmost2 = nmost;	
					most = amt[h];
					nmost = h;
				}
				else
				{
					if( amt[h] > most2 )
					{
						most2 = amt[h];
						nmost2 = h;
					}
				}
			}


			if( ok_on )
				printf( "\tcan be started on: %s; ", ok_on );

			move = "";
			#if( force && nok < 2 )				# would like a second option for the job
			#{
			#	stuck = 2;
			#	avoid = avoid " " ok_on " " nmost " ";
			#}

			if( stuck && jidx[job] )
			{
				if( nmost )			# try to shove files to the node that has the most bytes needed
				{
					printf( "%s/%s have most needed bytes; ", nmost, nmost2 );
					target = nmost;
				}
				else		# if we dont have a most bytes, then shove to a node that has the file, but not in avoid list
				{
					nlives = split( liveson[jneeds[job,0]], a, " ");
					target = ""
					for( i = 1; !target && i <= nlives; i++ )
					{
						if( !index( " " avoid " " , " " a[i] " " ) )		# not on avoid list and on ok-attr list
							if( ok_nattr[job] == " " || index( ok_nattr[job], " " a[i] " " ) )
								target = a[i];				# select this for target
					}
				}

				#if( must_on[job] && substr( must_on[job], 1, 1) != "!" )  # if a node MUST get the job, set that as target
				#{
					#target = must_on[job];
					#printf( "%s must get job; ", target );
				#}
				#else
				#{
					#if( force && nmost2 )		# force chooses the runner up
						#target = nmost2;
				#}

				if( target == "" )			# still no target selected, pull one out of our bottle
					target = random_node( job );

				printf( "\t%s %s target=%s ", avoid ? "avoid: " avoid : " ", stuck > 1 ? "FORCED" : "STUCK", target );
				if( verbose > 1 )
				{
					printf( "\n" );
						for( zz in amt )
							printf( "%s currently has %.0f bytes\n", zz, amt[zz]);
				}

				for( i = 0; i < jidx[job]; i++ )
				{
					if( liveson[jneeds[job,i]] )
					{
						printf( "\n\t%s %s (%s) ", jneeds[job,i], fsize[jneeds[job,i]],liveson[jneeds[job,i]] );
						#if( i > 0 && target )
						if( target )
						{
							if( ! index(  liveson[jneeds[job,i]] " ", target " "  ) )
							{
								printf( "<=== shift" );
								move = sprintf( "%snudge: %s %s\n", move, jneeds[job,i], target );
							}
						}
					}
					else
						printf( "\n\t%s (no copies) ",  jneeds[job,i], fsize[jneeds[job,i]] );
				}

			}

			printf( "\n" );
			if( move )
				printf( "%s", move );
		}

	}
'>/tmp/x.$$



if (( $verbose > 0 ))
then
	if (( $state_only > 0 ))		# just dump state to stdout and exit
	then
		cat /tmp/x.$$
		cleanup 0
	fi

	cat /tmp/x.$$ >&2
fi

if (( $forreal < 1 ))
then
	rm /tmp/*.$$
	cleanup 0
fi

awk '
	/nudge:/	{
		if( seen[$2] && seen[$2] != $NF )
			printf( "*** WARNING *** file directed toward two different nodes! %s (%s)\n", $0, seen[$2] ) >"/dev/fd/2";
		seen[$2] = $NF;
	
	}
'</tmp/x.$$

mkjoin >$TMP/$LOGNAME.j					# get all file names from the dump 5 and then send a dump 1 to repmgr
verbose "join file contains $(wc -l <$TMP/$LOGNAME.j) files"

# peek at the repmgr dump info and generate hint/hood commands that can be fed into repmgr via ng_rmreq
awk 	-v hint=$hint \
	-v jfile=$TMP/$LOGNAME.j \
	-v hint_lease=$hint_lease \
	'
	$1 == "nudge:" {
		file[$2":"] = $3;			# save file and target	
		need++;
	}

	END {
		oneed = need;
		while( (getline<jfile) > 0 )
		{
			if( $1 == "file" && file[$2] )
			{

				fname = substr( $2, 1, length($2)-1 );
				split( $3, a, "=" );
				if( hint )
					printf( "hint %s %s %.0f %s\n", a[2], fname, hint_lease, file[$2] );
				else
					printf( "hood %s %s %s\n", a[2], fname, file[$2] );
				if( --need < 0 )
					exit( 0 );
				file[$2] = "";
			}
		}

		if( need )
		{
			printf( "did not find all files! needed %d, found %d (%d missing)\n", oneed, oneed-need, need ) >"/dev/fd/2";
			for( f in file )
				if( file[f] )
					printf( "did not find: %s(%s)\n", f, file[f] ) >"/dev/fd/2";
		}
	}
'</tmp/x.$$

cleanup 0

exit

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_goad:Goad stuck files to common node)

&space
&synop	ng_goad [-a avoid-list] [-F] [-f dump-5-file] [-g] [-n] [-m must-file] [-o only-consider-list] [-s jobbernode] [-t sec] [-v|V]

&space
&desc	&This queries &ital(seneschal) for a list of files needed for pending jobs (via dump 5) and 
	generates a list of hint commands to resolve deadlocks.  A deadlock is a job that needs 
	multiple input files, which do not all reside on a common node.  &This will select a target 
	node, the node to which files not resident on the node, as the node containing the most 
	date in terms of bytes, not number of files.  The list of hint commands is written to 
	the standard output device which can be piped directly to &lit(ng_rmreq).

&space
	Under certian circumstances, some jobs must be run on a particular node regardless of the 
	location of existing files.  To force this, &this accepts a 'must run on' list that 
	lists the jobname and node name as the last to tokens on each record. When a job is identified
	in the must run on list, the specified node is targeted and hint commands generated for that 
	node regardless of the number of bytes tha would be transferred. 

&space
	Care must be taken when running &this during for &ital(gen-x) jobs.  These kinds of jobs 
	depend on partition and bundle lists which are replicated to all nodes.  If &this is executed
	before a good majority of the lists are replciated, the source of the lists is likely to be 
	targeted. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-a node : &This will try to avoid sending files to this node. If desired, node may be 
	 a blank separated list of node names. Specifying an avoid node provides an easy way 
	to force a job to run on a node other than the node that does have all of the input 
	files.  (See the force option).
&space
&term	-c n : The number of copies of a file that must be known to repmgr before &this will create
	a hint command to shift the file. If not specified, or n is zero, then a hint command is 
	generated for all files that are needed to be moved to unstick jobs.
&space
&term	-F : Force mode.  Forces the target to be a secondary node (one that does not already 
	have all of the files). This allows &this to be used to scatter files such that jobs 
	may run more evenly across the cluster. 
&space
&term	-f file : Provides the seneschal dump 5 information in a file. If not supplied, then &this 
	will query seneschal for the information. 
&space
&term	-g : Ignore goaway file.  &This will refuse to run if its goaway file is set (see ng_goaway).
	This overrides the check allowing a scheduled execution to abort, while still being able to 
	run &this from the command line. 
&space
&term	-h : Generate hood commands rather than hint commands. The default is to generate hints which 
	have no long lasting effect on the location of a file. 
&space
&term	-n : No execution mode. 
&space
&term	-m file : Supplies a list of job node pairs for jobs that must execute on a spcecific node. 
	This is particulary important for xtag jobs. 
&space
&term	-o pattern : Causes &this to consider only those jobs that match the pattern given. Convenient 
	when trying to limit the verbose output to look at, or to avoid considering jobs for which 
	file movement is not desired. If pattern is a node name, and -a node is also supplied, 
	the result (though matching not programming) is that for jobs that can run on the indicated node
	the files it needs are forced to pool on another node. 
&space
&term	-s node : The node running &ital(seneschal).
&space
&term	-t sec : Supply a timeout (seconds) to use while waiting for seneschal responses.  If 
	not supplied, 300 seconds (5 minutes) is used. 
&space
&term	-v : verbose mode. When set, a complete listing of the state and what must happen is 
	written to the standard error device. 
&space
&term	-V : State only.  Generates a verbose state listing to the standard output and quits. Does not 
	generate any hint commands.
	
&endterms

&space
&exit	Returns 0 to indicate a good exit code. 

&examp	The following illustrate the use of &this:
&space
&litblkb

  # generate hint commands only for xtag jobs
  ng_goad -o xtag -m /tmp/muston >/tmp/h 2>/tmp/v

  # generate hint commands only for pupdate jobs
  ng_goad -o pupdate -v >/tmp/h 2>/tmp/v

  # generate hint commands to force the move files that can run on bat04
  ng_goad  -F -a bat04 -o bat04 -v >/tmp/daniels.xh 2>/tmp/daniels.xp
&litblke

&space
&files	The hint list is written to standard output, and a list of the current state (where things 
	can be run, where files exist and what must be moved) is written to standard error.

&space
&see	&ital(ng_seneschal) &ital(ng_nawab)

&space
&bugs
	&This depends on &lit(ng_rm_join) and will need to be modified if/when replication manager 
	is changed to eliminate the dump file. 
&space
	If more than one node is given with the -a option, and all  files for a job reside on 
	the avoided nodes, then &this does not know what to do, and does not issue any 
	kind of diagnostic to that effect either.
&space
&mods
&owner(Scott Daniels)
&lgterms
&term	12 Oct 2003 (sd) : Thin end of the wedge.
&term	10 Dec 2003 (sd) : Added -c option. 
&term	17 Dec 2003 (sd) : Corrected issue with calculation of node with second most bytes of data.
&term	06 Aug 2004 (sd) : Made adjustment for the new seneschal which dumps needed input as pending input now.
&term	08 Aug 2004 (sd) : Corected test for avoid -- inverted parms in index(). 
&term	08 Nov 2004 (sd) : Added -t timeout option.
&term	06 Dec 2004 (sd) : corrected variable name in the avoid check.
&term	07 Dec 2004 (sd) : To set avoid to a null string (rather than the leader).
&term	06 Jun 2004 (sd) : Fixed bug with seneschal reporting <any>(node).
&term	14 Sep 2006 (sd) : Mod to man page. 
&term	22 Dec 2006 (sd) : Fixed to use dump1 interface with repmgr; also fixed problem with -o option.
&term	30 Apr 2007 (sd) : Now takes into account node attributes when a job is restricted to, or away from, 
		a node with a set of attributes.
&term	05 Jun 2008 (sd) : Fixed typo in ok_nattr array index; was causing jobs with a list of attributes
		to not select the node with most data as the target if it had the right attribute. 
&endterms

&scd_end
------------------------------------------------------------------ */
