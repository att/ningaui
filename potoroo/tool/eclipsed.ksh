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
# Abstract:	reports on variables that are shadowed  in pinkpages and
#		attributes that shadow repmgr variables.
#		
# Date:		27 Nov 2007
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN



# -------------------------------------------------------------------
ustring="[-a] [-p | -r] [-v]"
list_all=1
pinkpages=0
repmgr=0
all_conflicts=0			# set with -a to show all conflicts in pinkpages vars even if they are the same
ng_sysname | read mynode junk

while [[ $1 == -* ]]
do
	case $1 in 
		-a)	all_conflicts=1;;
		-p)	list_all=0; pinkpages=1;;
		-r)	list_all=0; repmgr=1;;

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

get_tmp_nm avfile rvfile nvfile nvsfile | read avfile rvfile nvfile nvsfile
if (( $list_all + $repmgr > 0 ))
then
	if [[ $srv_Filer != $mynode ]]
	then	
		ng_rcmd $srv_Filer 'gre ":=" $NG_ROOT/site/rm/dump' >$rvfile
	else
		gre ":=" $NG_ROOT/site/rm/dump >$rvfile
	fi

	for n in $( ng_members -N )
	do	
		echo "$n $(ng_rcmd $n ng_get_nattr)"
	done >$avfile

	awk -v all_conflicts=$all_conflicts -v avfile=$avfile '
		BEGIN {
			while( (getline<avfile) > 0 ) 		# expect nodename attr1 attr2...attrn
			{
				x  = split( $0, a, " " );
				for( i = 2; i <= x; i++ )
					attr[a[i]] = attr[a[i]] a[1] " ";
			}
			close( avfile );

			for( x in attr )
				attr[x] = substr( attr[x], 1, length( attr[x] ) - 1 );

			nshadowed = 0;
		}

		{
			if( attr[$1]  && (all_conflicts || attr[$1] != $3) )
			{
				printf( "repmgr variable shadowed by attribute: attribute=%s  node(s)=%s repmgr=%s\n", $1, attr[$1], $3 );
				nshadowed++;
			}
		}

		END {
			if( !nshadowed )
			{
				printf( "no repmgr variables are shadowed by attributes\n" );
				exit( 0 )
			}

			exit( 1 );
		}
	' <$rvfile
	rrc=$?
fi


echo " "

if (( $list_all  + $pinkpages > 0 ))
then
	ng_members | read members
	ng_nar_get | grep pinkpages >$nvfile

	gre "^pinkpages/flock" $nvfile >$nvsfile			# 'sort' by flock, cluster and then nodes
	gre "^pinkpages/$NG_CLUSTER:" $nvfile >>$nvsfile
	gre -v "^pinkpages/flock|^pinkpages/$NG_CLUSTER:" $nvfile >>$nvsfile

	awk 	-v all_conflicts=$all_conflicts \
		-v memlist="$members" \
		-v cluster=$NG_CLUSTER \
		'
		BEGIN {
			x = split( memlist, a, " " );
			for( i = 1; i <= x; i++ )
				members[a[i]]++; 
		}

		# return true if what is in the member list
		function ismember( what )
		{
			for( x in members )
				if( what == x )
					return 1;
			return 0;
		}

		{
			#expect lines like this:  pinkpages/kowari:srv_Jobber="value" #comment
			gsub( "^pinkpages/", "", $1 );
			gsub( "\"[ \t]*#.*", "\"", $0 );
			split( $0, a, "=" );			# scope:vname to a[1]
			split( a[1], b, ":" );			
			scope = b[1];
			var = b[2];

			if( scope == cluster || scope == "flock" ) 
			{
				if(  all_conflicts || (scope == cluster && value[var, "flock"] != a[2]) || scope == "flock" )
					count[var]++;
				value[var, scope] = a[2];
			}
			else
			{
				if( ismember( scope ) )			# save var/val just for members of our cluster
				{
					if( count[var] )		# count only if it shadows flock/cluster val
					{
						if( all_conflicts || 
							(value[var,"flock"] != "" && a[2] != value[var,"flock"]) || 
							(value[var,cluster] != "" && a[2] != value[var,cluster]) )
						{
							show_local[var, scope] = 1;
							count[var] = 3;	# force it to show if different	
						}
					}
							
					nodes[scope]++;
					nvalue[var":"scope] = a[2];
				}
			}
		}

		END {
			rc = 0;

			for( c in count )
			{
				space = 0;
				if( count[c] > 1 )
				{
					rc=1;

					if( all_conflicts || 
						(value[c,"flock"] != value[c,cluster] && value[c,"flock"] != "") || 
						count[c] > 2 )
						printf( "shadowed: %s \tf=%s c=%s \n", c, value[c,"flock"], value[c,cluster] );

					if( all_conflicts ||  count[c] > 2 )
						for( n in nodes )
							if( nvalue[c":"n] && show_local[c,n] )
							{
								printf( "\tlocal shadow: %10s: %s\n", n, nvalue[c":"n] );
								space = 1;
							}
				}				

				if( space )
					printf( "\n" );
			}

			exit( rc );
		}
	' <$nvsfile
	prc=$?
fi



cleanup $(( ${prc:-0} + ${rrc:-0} ))



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_eclipsed:Find repmgr or pinkpages variables that are shadowed by other things)

&space
&synop	ng_eclipsed [-a] [-p | -r] [-v]

&space
&desc	&This examines current replication manager and pinkpages variables and reports on 
	variables that are eclipsed.  Replication manager variables can be shadowed by 
	node attributes while pinkpages variables may be shadowed by values defined at a 'lower'
	scope level. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-a : List all conflicts.  If this is not supplied, only the conflicts that have different
	values are listed. 
&space
&term	-p : &This will examine only the pinkpages variables.
&space
&term	-r : Only the replication manager variables are examined.
&space
&term	-v : Might cause &this to be more verbose onto the standard error device. 
&endterms


&space
&exit	&This will exit with a non-zero return code if one or more variables is eclipsed. This allows
	&this to be used as a spyglass probe if desired. 

&space
&see	ng_spyglass, ng_ppget, ng_ppset, ng_nar_get, ng_nar_set, ng_repmgr, ng_get_nattr, ng_del_nattr, ng_add_nattr

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	28 Nov 2007 (sd) : Its beginning of time.  (HBDMAZ)
&term	01 Dec 2007 (sd) : Fixed exit code setting at end of script. Fixed bug which was causing 
		full variable value string to be chopped.
&term	04 Dec 2007 (sd) : Fixed search through sorted file.
&endterms


&scd_end
------------------------------------------------------------------ */

