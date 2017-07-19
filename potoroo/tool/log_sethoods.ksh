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
#!/ningaui/bin/ksh
# Mnemonic:	log_sethoods
# Abstract:	set and manage the hoods for mlog fragements and masterlog files in repmgr
#		
# Date:		22 Jan 2007
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


# drop hoods assigned to nodes that are on the drop_list
function drop_hoods
{
	typeset tlist="$(get_tmp_nm tlist)"	# spot for pruned list 

	if [[ -z $drop_list ]]
	then
		return
	fi

	typeset list="${drop_list% }"		# drop last blank

	
	gre -v "${list// /|}" $hood_list >$tlist		# prune out current settings for dropped nodes
	typeset before=""
	typeset after=""

	wc -l <$hood_list | read before
	wc -l <$tlist | read after
	verbose "$(( $before - $after )) hoods will be reassigned for dropped nodes: $drop_list"

	mv $tlist $hood_list
}

# -------------------------------------------------------------------
ustring="[-a add-list] [-b] [-d drop-list] [-h] [-i attribute] [-n] [-p lprefix] [-P fprefix] [-v]"
ignore_species="!Satellite !Isolated !NoMlog"	# we dont want to put things out on satellite, isolated or nomlog nodes; exclude from members list
forreal=1			# -n turns this off and prevents us from sending cmds to repmgr
ok2balance=0;			# -b sets this to allow shuffling in an effort to balance
gen_hood_cmds=0			# we dont usually generate the commands to alter the registrations; -h turns this on
mlprefix=masterlog.${NG_CLUSTER:-bad_cluster}		# prefix for masterlog files and hoods
frag_prefix=mlog
drop_list=""				# list of nodes that are leaving the cluster and must be dropped
add_list=""				# list of nodes that are joining the cluster and we want to force them to play as a rebalancing

while [[ $1 = -* ]]
do
	case $1 in 
		-a)	add_list="$add_list$2 "; shift;;
		-b)	ok2balance=1;;
		-d)	drop_list="$drop_list$2 "; shift;;
		-h)	gen_hood_cmds=1;;
		-i)	ignore_species="$2"; shift;; 
		-n)	forreal=0;;
		-p)	mlprefix=$2; shift;;			# keep -p and -P consistant with other log_ functions
		-P)	frag_prefix=$2; shift;;

		-v)	verbose=$(($verbose+1));;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

verbose "$argv0 starting"

# --------------- compute values of this and next weeks master log file names --------------
dow=$( ng_dt -w )			# today's day of the week (0==sun)
today=$(ng_dt -d )
today=${today%??????}			# yyyymmdd
midnight=${today}000000
midnight_ts=$( ng_dt -i $midnight )	# most recenltly passed midnight
lease_expiry=$(( $(ng_dt -i ) + 864000 ))	# hints require a lease expiration timestamp

if (( $dow == 0 ))
then
	dow=7				# push sunday to end of week 
fi

cts=$(( $midnight_ts - (864000 * ($dow - 1)) ))		# midnight of previous monday
nts=$(( $cts + (864000 * 7) ))			# timestamp of next monday

current=$(ng_dt -d $cts)
next=$(ng_dt -d $nts ) 				# date string of next monday (preset the hood)

#-------------------------------------------------------------------------------------------

#hood_list=$TMP/logger_hoods.list.$$	 	# tmp files for bits of info 
#df_list=$TMP/logger_hoods.df.$$
#flist=$TMP/logger_hoods.flist.$$
#frag_list=$TMP/logger_hoods.frag_list.$$
#hood_cmds=$TMP/logger_hoods.cmds.$$		# commands needed to set hoods

# get tmp file names
tpf=logger_hoods			# prefix for all tmp file names
get_tmp_nm $tpf.list $tpf.df $tpf.flist  | read hood_list df_list flist
get_tmp_nm $tpf.frag_list $tpf.cmds  | read frag_list hood_cmds

typeset -u hood_prefix=${mlprefix%.*}		# hood must be in upper case (repmgr requirement) -- must lose the cluster name

members="$(ng_species -a $ignore_species)"

if [[ -z $members ]]
then
	log_msg "abort: cannot get members list"
	cleanup 1
fi


# ----- gather a list of hood settings, df info and a masterlog file list ----------
# 	files are sorted by date, newest first, so that we can balance starting with newer files
#	the awk programme reads each of these data bits as it needs them
#
ng_rcmd  $srv_Filer 'gre := $NG_ROOT/site/rm/dump' | gre $hood_prefix >$hood_list
ng_rm_df |gre "%$" | gre -v Total >$df_list
ng_rm_where -5 -s -p -n "$mlprefix.[0-9]+000000$" | awk '{x = split( $2, a, "/" ); print $1, a[x], $(NF-1), $NF; }' |sort -u -r -k 2,2 >$flist
ng_rm_where -5 -s -p -n "$frag_prefix.*[0-9]+000000$" | awk '{x = split( $2, a, "/" ); print $1, a[x], $(NF-1), $NF; }' |sort -u -r -k 2,2 >$frag_list

cp $flist /tmp/daniels.f

drop_hoods $drop_list			# drop any hoods assigned to node(s) listed; modifies hood_list

awk 	-v members_str="$members" \
	-v drop_str="${drop_list% }" \
	-v add_str="${add_list% }" \
	-v df_list="$df_list" \
	-v hood_list="$hood_list" \
	-v flist="$flist" \
	-v fraglist="$frag_list" \
	-v fn_next=$next \
	-v fn_current=$current \
	-v ok2balance="$ok2balance" \
	-v gen_hood_cmds=$gen_hood_cmds \
	-v verbose=$verbose \
	-v mlprefix="$mlprefix" \
	-v hood_prefix="$hood_prefix" \
	-v expiry=$lease_expiry \
' 
	# ng_rm_df values come with a G, or T suffix; convert to some common set of units
	function normalise( x,		c )
	{
		c = substr( x, length(x)-1, 1 );
		if( c == "G" )
			return x+0;

		return x * 1024;
	}

	# find the node with the least amount assigned to it
	# node must be in the ok4hood list. this keeps sattelites from 
	# getting hoods even if a file replicates out there by accident
	function find_small( 		x, min, node, tries )
	{
		min = 9e9;
		node = "";

		for( x in node_size )
		{
			if( x != exclude_node  &&  ok4hood[x]  && node_size[x] < min )
			{
				node = x;
				min = node_size[x];
			}
		}

		if( ! node )		# none with size, or all the same, just pick
		{
			tries = 0;			# prevent loop if exclude node is the only node
			while( (node = members_list[int( rand() * nmembers ) + 1]) == exclude_node )
				if( tries++ > 10 )
					return node; 
		}
		return node;
	}

	# attempts to shift some of the load slecting more recent files if possible
	# we assign the hood to the node with the least assigned and hint that the 
	# file leave the overburdened node
	function drop_load( node, amount, 	i, nmoved, smoved )
	{
		nmoved = smoved = 0;

		exclude_node = node;				# prevent find smallest from finding our node

		# by fiat, the two most recent files are not moved; they are anchored to what had the most space when assigned
		for( i = 2; i < nfiles && amount -fsize[forder[i]] > 0; i++ )
		{
			if( hood[forder[i]] == node )
			{
				printf( "hint %s %s.%s %s !%s\n", md5[forder[i]], mlprefix, forder[i],  expiry, node );
				hood[forder[i]] = "";
				hood[forder[i]] = create_hood( forder[i], random );
				node_size[node] -= fsize[forder[i]];
				#tot_fsize -= fsize[forder[i]];
				assigned[node]--;
				amount -= fsize[forder[i]];
				smoved += fsize[forder[i]];
				nmoved++;
			}
		}

		exclude_node = "foo";
		printf( "drop_load: %s %d(files) %dM(amt) moved away\n", node, nmoved, smoved ) >"/dev/fd/2";
	}

	# select a node for a hood, and generate the command to do it; hint is how to make the assignment:
	# based on free/most space, or randomly
	function create_hood( name, hint, 		node )
	{
		ncreated++;

		if( hint == random )
			node = find_small( );			# greedy to the one with the least
		else
		if( hint == hood2free )
			node = max_free_node;
		else
			node = max_total_node;

		if( gen_hood_cmds && md5[name] )
			printf( "hood %s %s.%s %s_%s\n", md5[name], mlprefix, name, hood_prefix, name );

		set_cmds[name] = sprintf( "set %s_%s %s", hood_prefix, name, node );		# collect them so we do not set and reset during balancing

		assigned[node]++;
		node_size[node] += fsize[name];		
		#tot_fsize += fsize[name];		

		return node;
	}
	
	# check the current distribution and if out of whack (more than 20% from the mean),
	# and allowed to balance, then generate hint commands to balance things a bit better
	# we call this function as many as three times to give a before and after set of 
	# stats as well as actually doing the blancing.
	function check_balance( ok2doit, annotate, 		 mean, max, min, node )
	{
		min = 9e9;
		max = 0;

		mean = tot_fsize/nmembers;

		min_node = max_node = "";
		for( node in node_size )
		{
			deviation[node] = d = ((node_size[node] - mean)/mean ) * 100;

			if( annotate )
				printf( "stats: %s %.2fM(size) %.2f%%(deviation) %d(files) %s\n", node, node_size[node], d, assigned[node], (d > btollerate)  ? "[ADJUST]" : "" ) >"/dev/fd/2";

			if( deviation[node] > btollerate  && ok2doit )
				drop_load( node, int( mean * (d/100) ) );

			if( min > node_size[node] )
			{
				min = node_size[node];
				min_node = node;
			}
			if( max < node_size[node] )
			{
				max = node_size[node];
				max_node = node;
			}
		}

		if( annotate )
		{
			high_dev = ((node_size[max_node] - mean)/mean)*100;
			low_dev  = ((node_size[min_node] - mean)/mean)*100;

			printf( "stats: %.3fM(total) %d(nodes) %.2fM/node(mean) %0.2fM(max) %.2f%%(max-dev) %.2fM(min) %.2f%%(min-dev)\n", tot_fsize, nmembers, mean, max, high_dev, min, low_dev, mean  ) >"/dev/fd/2";
		}
	}

	BEGIN {
		srand();

		btollerate = 20;		# when a node is out of whack by more than this percentage, we balance it

		one_meg = 1024 * 1024;
		random = 0;		# constants used to suggest where a hood should be created
		hood2free = 1;
		hood2total = 2;
		
		ncreated = 0;		# number of hoods created
		nhoods = 0;
		nfiles = 0;
		nmembers = 0;

		nmembers = split( members_str, members_list, " " );
		for( i = 1; i <= nmembers; i++ )
		{
			members[members_list[i]] = 1;			# index on name
			node_size[members_list[i]] = 0;
			ok4hood[members_list[i]] = 1;			# if not on the origial members list, they cannot get a hood
		}

		ndrop = split( drop_str, drop_list, " " );
		for( i = 1; i <= ndrop; i++ )
			ok4hood[drop_list[i]] = 0;		# turn off their flag

		nadd = split( add_str, add_list, " " );
		if( nadd > 0 )
			btollerate = 5;			# try to force a reordering of sorts

		for( i = 1; i <= nadd; i++ )
		{
			if( ! ok4hood[add_list[i]] )		# not already known to us
			{
				# dont need to force a df_* value as they are used only to set future (next weeks) hoods
				members[add_list[i]] = 1;
				ok4hood[add_list[i]] = 1;		# turn on their flag -- they might not be in members yet
				node_size[add_list[i]] = 0;
			}
		}

		nmembers += nadd - ndrop;			# only count number that we can assign hoods to

		max_total = max_free = 0;			# index to the max values of each 
		i = 0;
		while( (getline < df_list) > 0 )		# pick up node with most repmgr space
		{
			if( members[$1] )			# only pick up those in members 
			{
				df_name[i] = $1;
				df_free[i] = normalise($4);
				df_total[i] = normalise($2);
				if( df_total[i] > df_total[max_total] )
					max_total = i;
				if( df_free[i] > df_free[max_free] )
					max_free = i;
	
				i++;
			}
		}
		close( df_list );

		max_total_node = df_name[max_total];		# name of each node with most type of space
		max_free_node = df_name[max_free];

		while( (getline < hood_list) > 0 )
		{
			nhoods++;
			if( split( $1, a, "_" ) > 1 )			# we just want the date
			{
				hood[a[2]] = $3;			# x := y
				assigned[$3]++;
			}
		}
		close( hood_list );

		while( (getline <fraglist) > 0 )		# fragements -- ensure there is a hood assigned for them 
		{
			x = split( $2, a, "." );
			if( substr( a[x], 1, 1 )+ 0 > 0 )		# must be a year
				ensure[a[x]] = 1;
		}
		close( fraglist );

		# assume format is: ningd7 /ningaui/fs03/03/25/masterlog.20030609000000 <md5-value> <size>
		# and that files are sorted by date (newest first)
		fidx = 0;
		while( (getline < flist) > 0 )
		{
			xx = split( $2, a, "." );			# index only by date (assumed last)
			if( a[xx] )
			{

				if( ! fsize[a[xx]] )				# multiples in the list, some info is needed just once
				{
					forder[fidx++] = a[xx];			# order of files for balancing
					nfiles++;				# unique files
					md5[a[xx]] = $(NF-1);			# need md5 if we must hint to repmgr
					fsize[a[xx]] = $NF/one_meg;
					tot_fsize += fsize[a[xx]];
				}
				if( hood[a[xx]] )
					node_size[hood[a[xx]]] += $NF/one_meg;	# rack up the size of stuff assigned to node
				files[a[xx]] = files[a[xx]] $1 " ";		# file local may be important  so save it
			}
			else
				printf( "bad file: %s\n", $0 ) >"/dev/fd/2";
		}
		close( flist );

		ncreated = 0;

		# -------------------- start real work --------------------------------------------

		nreassigned = 0;
		for( h in hood )		 # determine if any hoods map to non-existant nodes ----------------------
		{
			if( hood[h] && ! members[hood[h]]  )			# hood value not listed in member
			{
				assigned[hood[h]]--;
				hood[h] = "";					# delete here; it will be reallocated later (after any balancing)
				nreassigned++;
			}
		}		

		if( ! hood[fn_current] )			# set hoods for most recent and next week ----------------
			hood[fn_current] = create_hood( fn_current, hood2free );

		if( ! hood[fn_next] )
			hood[fn_next] = create_hood( fn_next, hood2free );

		for( f in files ) 				# create hoods for each file that did not have one -------
		{
			if( f && ! hood[f] )				# not a hood for the file
				hood[f] = create_hood( f, random );
		}
		for( f in ensure )
		{
			if( f && ! hood[f] )				# not a hood for the file
				hood[f] = create_hood( f, random );
		}

		hold_created = ncreated;		# balancing calls create hood so we need to zero counter to separate kinds
		ncreated = 0;
		if( nhoods > nmembers )
		{
			if( ok2balance )		# if ok, then do it 
			{
				if( verbose > 1 )
				{
					printf( "prebalance stats: \n" ) >"/dev/fd/2";
					check_balance( 0, 1 );		# peek and list the current state before balancing
				}

				check_balance( ok2balance, 0 );	# now actually do it; we wait until the end to show results
			}
		}

		for( x in set_cmds )			# write the repmgr set commands
			printf( "%s\n", set_cmds[x] );

		nmoved += ncreated;			# moved because of balancing
		ncreated = hold_created;
		# -------------------- end housekeeping and generate closing statsitcs -------------
		printf( "hoods already in repmgr: %4d\n", nhoods ) >"/dev/fd/2";
		printf( "          hoods created: %4d\n", ncreated ) >"/dev/fd/2";
		printf( "hoods moved (balancing): %4d\n", nmoved ) >"/dev/fd/2";
		printf( "       hoods reassigned: %4d\n", nreassigned ) >"/dev/fd/2";
		printf( "       master log files: %4d\n", nfiles ) >"/dev/fd/2";
		printf( "        cluster members: %4d\n", nmembers ) >"/dev/fd/2";
		printf( "\n" ) >"/dev/fd/2";

		if( verbose )
			check_balance( 0, 1 );		# give a final summary of things
	}
' >$hood_cmds


rc=0
if (( $forreal > 0 ))
then
	verbose "submitting hood commands"
	ng_rmreq <$hood_cmds
	rc=$?
	mv $hood_cmds $TMP/log_sethoods.rm.cmds
else
	verbose "hood commands NOT submitted (no execute mode)"
	cat $hood_cmds
fi

cleanup $rc




/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_log_sethoods:Set repmgr hoods for masterlog files)

&space
&synop	ng_log_sethoods [-a add-list] [-b] [-d drop-list] [-h] 
&break	[-i attribute] [-n] [-p lprefix] [-P fprefix] [-v]

&space
&desc	&this reads a list of current replication manager variable settings, 
	and the list of master log files, then generates any needed variable 
	commands to set the hood value(s) for those master log files without
	a current setting. 
&space
	We expect that master log files are kept in replication manager space 
	with the filename syntax of &lit(masterlog_yyyymmdd000000), and that 
	hoods are set using only the date portion of the filename. Further, 
	this script assumes that the appropriate hood value is set in 
	replication manager when the file is registered; &this makes no 
	attempt to set values for file registrations. 
&space
&subcat Balancing
	When &this is executed in the balancing mode, the current distribution 
	of master log files, according to the hood values (the replicated 
	location is not considered) is analysed and if a node has more than 
	20% of the mean distribution, then hood assignments are adjusted. 
	For each hood that is marked for adjustment, the hood is assigned 
	to the node with the least amount of masterlog data, and a &ital(hint)
	is given to replication manager to move the existing master log file
	off of the previously assigned node. 


&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-a node : One or more node names that are to be added to the members list. This 
	allows new nodes to be assigned hoods before, or as, they join the cluster. 
&space
&term	-b : Balance mode; causes the commands necessary to shift hood assignments to be 
	created and sent to repmgr.
&space
&term	-d node : One or more node names for which hoods are to be deleted.  Generally 
	this option is used as a node is removed (isolated) from the cluster. 
&space
&term	-h : Generate hood commands for file registrations. Normally &this does not generate
	any replication manager commands that affect the file registrations. This causes those
	commands to be generated.  This is probably needed only when converting existing 
	registrations that listed &lit(Logger) as the hood to a specific date based hood.
&space
&term	-i species : The species of node(s) that should be ignored. If not supplied this defaults
	to &lit(!Satellite, NoMlog,) and &lit(Isolated) which usually should be the correct list
	of node types that should not be given masterlog files. 
&space
&term	-n : No execute mode. Looks at what needs to be done, and reports on it without 
	actually sending the commands to repmgr.  The commands that are sent are written
	to standard output; stats are always written to standard error. 
&space
&term	-p prefix : Supplies the prefix used for masterlog files and neighbourhood names. If not 
	supplied "masterlog" is assumed. 
&space
&term	-P fprefix : Supplies the prefix for mlog fragment files. If not supplied, mlog is assumed.
&space
&term	 -v : Causes &this to chat about what it is doing.  
&endterms


&space
&exit	An exit code of 0 indicates success. 

&space
&see	ng_log_frag, ng_log_comb, ng_repmgr

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	22 Jan 2007 (sd) : Its beginning of time. 
&term	19 Feb 2007 (sd) : Added check for missing hoods that are also missing masterlog
		files (have mlog files). Mostly to deal with buggered (orphaned) masterlog
		files from version 1.
&term	22 Feb 2007 (sd) : Changed to prevent Sattelite nodes from being assigned hoods 
		even if a hood was accidently assigned previously.
&term	20 Mar 2007 (sd) : Pulled reference to the Logger attribute from man page. 
&term	26 Mar 2007 (sd) : Corrected the filename generated with a hint. It had an underbar
		rather than a dot and this was causing ghosts registrations with a size of 
		-1 in repmgr. 
&term	11 May 2007 (sd) : Now avoids nodes that have the Isolated attribute.  Added the 
		-a and -d options.
&term	28 Jun 2007 (sd) : Fixed problem with tollerance adjustment calculation (typo).
&term	19 Aug 2008 (sd) : Added NoMlog to the list of node attributes that cause a node to 
		not be the target of any masterlog files. 
&term	20 Mar 2009 (sd) : Added cluster name to the filename in the repmgr area. 
&term	20 Apr 2009 (sd) : Corrected problem introduced when clustername was added to the 
		filename. 
&endterms


&scd_end
------------------------------------------------------------------ */

