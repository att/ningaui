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
# Mnemonic:	nar_tree_sanity
# Abstract:	check the state of the narbalek tree. We toss in a variable, wait a 
#		small amount of time, and then test the variable on each node. 
#		disconnected nodes will not have the right value and we will force them
#		to restart.
#		
# Date:		11 May 2006
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# build a list of pinkpages variables that we must find in pp_get dump and set 
# count; this makes it easy to add new ones and no manual count must be maintained
function build_pp_list
{
	pp_count=0
	(
	cat <<endKat
		NG_ROOT
		srv_Jobber
		srv_Filer
		srv_Steward
		srv_Netif
		PATH
		NG_RMBT_PORT
		NG_NAWAB_PORT
		NG_SRVM_PORT
		NG_RMDB_PORT
		NG_WOOMERA_PORT
		NG_LOGGER_PORT
		NG_REPMGRS_PORT
		NG_NARBALEK_PORT
		NG_PARROT_PORT
		NG_SENESCHAL_PORT
		NG_SHUNT_PORT
		NG_SPYG_PORT
		NG_NNSD_PORT
		NG_D1AGENT_PORT 
endKat
	) | while read vname
	do
		if [[ -n $list ]]
		then
			list="$list|^$vname="
		else
			list="^$vname="
		fi
		pp_count=$(( $pp_count + 1 ))
	done

	echo "$list"
	return $pp_count		# pp_count does not get set globally so as long as there are not more than 255 vars, this works
}

function ck_data_sanity
{
	typeset now_data=""
	typeset last_data="$TMP/$user.nar_ck.last_data"
	typeset lcount=""
	typeset ncount=""
	typeset lost=""
	typeset last_sorted=""
	typeset now_sorted=""
	typeset rc=0
	typeset members=""
	typeset friend=""
	typeset friend_count=0
	typeset need_tree=0

	get_tmp_nm now_data now_sorted last_sorted | read now_data now_sorted last_sorted

	members="$(ng_members -N)"				# find a neighbour in the cluster and compare their count to ours
	if [[ $members == "$host"* ]]
	then
		friend="${members#* }"				# strip our name
		friend="${friend%% *}"	# take first from the remainder
	else
		eval friend="\${members%% $host*}"
		friend="${friend##* }"				# take the one just before our name
	fi
	
	ng_rcmd $friend 'ng_nar_get|wc -l' | read friend_count
	
	ng_nar_get >$now_data

	if [[ ! -f $last_data ]]
	then
		log_msg "no previous capure of data is available; data from this invocation will be saved ($last_data) [OK]"
		mv $now_data $last_data 
		return 0
	fi

	wc -l <$now_data | read ncount
	wc -l <$last_data | read lcount

	if (( ${friend_count:-0} > 0 ))
	then
		fdiff=$(( $friend_count - $ncount ))
		if (( $fdiff > $lost_threshold || $fdiff < -$lost_threshold ))
		then
			log_msg "narbalek variable count differs significantly from neighbour: $friend=$friend_count us=$ncount [FAIL]"
			need_tree=1
			rc=1
		else
			log_msg "narbalek variable count similar to neighbour: $friend=$friend_count us=$ncount [OK]"
		fi
	else
			log_msg "neighbour node $friend appears down; similar count not checked" 
	fi

	if (( $lcount > $ncount ))		# we lost some
	then
		lost=$(( $lcount - $ncount ))	# how much
		if (( $lost > $lost_threshold ))
		then
			log_msg "narbalek variable count is down significantly: tollerance=$lost_threshold lost=$lost before=$lcount now=$ncount [FAIL]"
			need_tree=1

			awk '{gsub( "=.*", "" ); print}' <$last_data |sort >$last_sorted
			awk '{gsub( "=.*", "" ); print}' <$now_data |sort >$now_sorted
			log_msg "variable name differences between last check (<) and now (>) "
			diff $last_sorted $now_sorted
			ts=$( ng_dt -i )
			if [[ -d $TMP/$user ]]		# for spyglass, nice to preserve a copy of the data
			then
				log_msg "holding data files in: $TMP/$user/nar_ckdata.*.$ts"
				cp $last_data $TMP/$user/nar_ckdata.last_data.$ts
				cp $now_data $TMP/$user/nar_ckdata.now_data.$ts
			fi

			rc=1
		else
			log_msg "narbalek variable count is within tolerances: lost=$lost before=$lcount now=$ncount [OK]"
		fi
	else
		log_msg "narbalek variable count is within tolerances: before=$lcount now=$ncount [OK]"
		ng_ppget >$now_sorted

		vlist="$( build_pp_list )"
		pp_count=$?				
		gre -c "$vlist" $now_sorted | read count 
		if (( $count != $pp_count ))
		then
			log_msg "search of pinkpages data did not yield expected count of important variables: wanted=$pp_count got=$count [FAIL]"
			log_msg "searching for: ${vlist//=|^/ }"
			log_msg "results:"
			gre "$vlist" $now_sorted |sort
			need_tree=1
			rc=1
		else
			log_msg "search of pinkpages data yielded expected count of important variables: wanted=$pp_count got=$count [OK]"
		fi
	fi

	if (( $need_tree > 0 ))
	then
		log_msg "current tree:"
		ng_nar_map -t >&2
	fi

	mv $now_data $last_data 

	return $rc
}


# -------------------------------------------------------------------
ustring="[-c] [-d] [-t n] [-u name] [-v]"
cluster_name="."
lost_threshold=50
ck_data=0
user=$USER
get_tmp_nm treef narmapf | read treef narmapf

while [[ $1 = -* ]]
do
	case $1 in 
		-c)	cluster_name=$NG_CLUSTER;;	# limit check to nodes in this cluster 
		-d)	ck_data=1;;
		-t)	lost_threshold=$2; shift;;
		-u)	user=$2; shift;;		# let spyglass set 

		-v)	verbose=$((verbose + 1));;
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

rc=0					# final exit code; set to 1 if we detect an error
host=$( ng_sysname )

if (( $ck_data > 0 ))
then
	ck_data_sanity
	cleanup $?
fi

token_name="nar_sanity/$host.$$"	# keep from colliding with others
token_value="$( ng_dt -i )"		# unique token for testing

verbose "posting token into narbalek: $token_name=$token_value"
ng_nar_set $token_name=$token_value

# do a bit of other work while we wait for propigation of the token 
ng_nar_map >$narmapf   #$TMP/nar_map.$$			# we use this to get a list of nodes to test, and to look for multiple roots
ng_nar_map -t >$treef &					# we will wait for this later if we need it
nar_map_pid=$!

if ! awk -v verbose=$verbose -v need="$cluster_name" ' 
	/I_AM_ROOT/ { 
		stuff[rcount+0] = $0;
		rcount++; 
	} 
	{ 
		x = index( $6, "(" );					# pull cluster name
		if( need == "." || substr( $6, 1, x-1 ) == need ) 
			print $3, $5; 
	} 
	END{ 
		if( rcount > 1 ) 	
		{
			if( verbose )
				for( i = 0; i < rcount; i++ )
					printf( "%s\n", stuff[i] ) >"/dev/fd/2";
			exit(1)
		}
	} 
	' <$narmapf >$TMP/node_list.$$
then
	log_msg "multiple nodes think they are the root of the tree"
	gre ROOT $narmapf >&2
	rc=1
fi


# should take a couple of seconds to get the nar_map processed, so we can assume token is wide spread by now
verbose "testing nodes"

ok=0
bad=0
ocount=0		# orphaned
while read n parent
do
	if [[ $parent == "ORPHANED"* ]]				# dont test orphans
	then
		ocount=$(( $ocount + 1 ))
	else
		ng_rcmd $n ng_nar_get $token_name | read pvalue
		if [[ $pvalue != $token_value ]]
		then
			bad=$(( $bad + 1 ))
			log_msg "disconnected node? $n   value=$pvalue expected=$token_value"
			# narreq.ksh -s $n -e
			rc=1
		else
			ok=$(( $ok + 1 ))
		fi
	fi
done <$TMP/node_list.$$ 


log_msg "ok nodes=$ok   bad nodes=$bad    orphans=$ocount"
ng_nar_set $token_name=""

if (( $rc != 0 ))  &&  [[ $user == spyglass ]]
then
	wait $nar_map_pid
	cat $treef >&2			# generate a tree as additional documentation 
	cat $narmapf	
fi
cleanup $rc



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_nar_tree_sanity:Determine if narbalek's tree is sane)

&space
&synop	ng_nar_tree_sanity [-c] [-d] [-t n] [-u name] [-v]

&space
&desc	&This queries all running narbalek processes for a status dump and then 
	examines the output to determine if there is more than one declared 
	'root' process. &This also introduces a test variable into the narbalek
	name space (not pinkpages) and checks to see if the value is propigated 
	to all nodes.  If either test fails, the exit code is non-zero.

&space
	If the data check option (-d) is set, then &this will attempt to do some 
	sanity checking with regard to the data that is inside of narbalek. 
	These things are checked:
&beglist
&item	The count of variables contained within the local narbalek is compared to 
	the count maintained by a narbalek on another node in the cluster. If the 
	difference is greater than 10, then an alarm is raised.
&space
&item	The number of narbalek variables currently contained inside of narbalek is 
	compared against the variables that were counted during the previous execution
	of &this. If the difference is greater than 50, an alarm is raised. 
&space
&item	A small set of pinkpages variables is looked for in the narbalek data. These
	variables are considered to be manditory and if any are missing it is assumed
	that the data in narbalek is corrupt.
&endlist

&space
	Data that is saved is placed into files that are timestampped.  This will prevent
	a rerun of the command from destroying data that could otherwise prove useful
	in determining the cause of a problem. Files that are saved are placed into 
	&lit($TMP/$USER). 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-c : Examine only nodes on the current cluster for the test value. If omitted, 
	all nodes responding to the data query are examined.  (ignored if -d supplied.)
&space
&term	-d : Check data rather than tree organisation.
&space
&term	-t n : Set the threshold for data checking to n.  If not supplied, &this will tollerate
	as many as 50 variable deletes since the last execution without raising an alarm.
&space
&term	-u name : Use name instead of $USER when saving data files if there is an error. This
	allows daemons like spyglass to have data saved in a 'non-user' oriented directory.
&space
&term	-v : Verbose mode.
&endterms


&space
&exit	An exit code indicates that one or both tests produced suspect results and 
	the state of narbalek should be examined. 

&space
&see	ng_narbalek, ng_spyglass

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	28 Jun 2006 (sd) : Its beginning of time. 
&term	02 Jul 2007 (sd) : Added check for orphan state -- we do not test for token on orphans as we dont
		expect that the token got there. If multiple nodes have declared themselves as root, we
		now write their data line to stderr.

&term	23 Aug 2007 (sd) : Added -d support.
&term	30 Oct 2007 (sd) : Added default to friend_count var -- wc retruns an empty string and not 0. 
&term	03 Dec 2007 (sd) : Removed check for RM_PORT as that var is now deprecated. 
&term	03 Jan 2008 (sd) : Now generates a tree map if sanity check fails and the user is spyglass and 
		captures the raw dump messages for analysis if we report a failure to spyglass.
&term	26 Jan 2008 (sd) : Corrected typo that was causing temp file name creation to fail.

&scd_end
------------------------------------------------------------------ */
