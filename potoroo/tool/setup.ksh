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
# --------------------------------------------------------------------------------------------
# Mnemonic:	setup
# Abstract:	setup a new node. must be run after all filesystems are created
# 		and mounted -- otherwise it creates directories where they should be.
#		Note: There is no scd in this script as tfm is likely not available in the 
#			path when this is executed.  
#
# Date:		Formalised on 6 August 2003 (HANN EKD/ESD #27)
# Author:	E. Scott Daniels
# Mod:		18 Sep 2003 (sd) - Better checking round cartulary.cluster so that 
#			we dont create cartulary.cluster if the real one exists.
#		30 Jun 2004 (sd) - adjusted min/node/cluster cartulary contents
#		08 Feb 2005 (sd) - To allow for the way prodCVE wants to set up a master node
#		24 Mar 2005 (sd) - Removed check for /usr/bin/ng_wrapper.
#		20 Dec 2005 (sd) - Changes to support new narbalek with nnsd scheme.
#		16 Jan 2006 (sd) - Added call to setup_common to ensure that links are set.
#		18 Feb 2006 (sd) - Added ability to link /ningaui/bin/ksh to /bin/ksh if
#			ast/common not installed/
#		05 Sep 2006 (sd) : Major revamp for release. 
#		10 Oct 2006 (sd) : Fixed problem with running narbalek (needed nnsdlist exported)
#		03 Apr 2007 (sd) : Fixed call to pp_cluster, added -N, -S and -u options
#		12 Apr 2007 (sd) : Added some things to pretty the output and to set repmgr 
#			tumbler if not set. 
#		01 May 2007 (sd) : Added srv_d1agent initialisation.
#		30 May 2007 (sd) : Changes to support sun differences. 
#		31 May 2007 (sd) : Now set the logger port to 29097 for satellite nodes as they
#			cannot use the clusters port on the off chance that they are ever connected
#			to the same broadcast net as the cluster.
#		16 Oct 2007 (sd) : Added support for -U and to assign NG_PROD_ID pinkpages var.
#		03 Dec 2007 (sd) : We now call the individual ng_srv command to do its own init.
#		05 Dec 2007 (sd) : Added better initialisation of cluster default pp vars.
#		07 Dec 2007 (sd) : Changes to allow setup to run on sun nodes which seem to still have
#			version of awk and kshell that are older than dirt. Script now checks for these
#			and aborts if it cannot tollerate what it finds. awk may be converted to nawk
#			if that seems to work, but if ksh does not support ${x//foo/bar} substitution
#			then we bail. 
#		20 Feb 2007 (sd) : Now calls ng_fix_hashbang to set things right; needed if package 
#			was built on a node that uses /ningaui/bin but that's not inuse on the 
#			target node. 
#		21 Feb 2008 (sd) : Ensure that site/log is created early.
#		17 Mar 2008 (sd) : Now sets LANG=C as a default in pinkpages
#		28 Mar 2008 (sd) : Cartulary is now created in NG_ROOT/site/cartulary.
#		12 Jun 2008 (sd) : Vars we set in site/cartulary.min are commented with date and tag to indicate we set them.
#		18 Jun 2008 (sd) : Added support to set narbalek league.
#		21 Jun 2008 (sd) : Cartulary is no longer in site, back to NG_ROOT since we are mounting in /l now.
#		10 Jul 2008 (sd) : Prod id is added to the site/cartulary.min if not ningaui.
#		30 Jul 2008 (sd) : Changes needed to run on node with bad awk/gawk, and to properly set links to /l (or 
#			other system local mount dir).  
#		04 Aug 2008 (sd) : Added RUBYLIB to default list of pp vars to set
#		06 Aug 2008 (sd) : Modified search for usable gawk/awk; added warnings if gawk not installed.
#		02 Sep 2008 (sd) : Added clarification to prod id prompt. Ensured that stuff written to cart.min is quoted.
#		14 Oct 2008 (sd) : We now set NG_PROD_ID in pinkpages even if it is ningaui.
#		13 Nov 2008 (sd) : Do not need to set NG_PROD_ID in rc.d -- site prep does this.
#		09 Feb 2009 (sd) : Hard set cart var in cart_min and narbalek to avoid getting cluster value when ng-root is 
#			different for the node
#		10 Mar 2009 (sd) : Corrected some pp var setting when vars are being set only for the local node. 
#				logger port, cartulary and tmp were screwwing things up.
#		18 May 2009 (sd) : We now capture TMP in cart.min only if -t is used. 
#		13 Aug 2009 (sd) : Now issues a warning, and doesn't set NG_DOMAIN, if a fully 
#			qualified hostname cannot be found. 
#		14 Aug 2009 (sd) : Added an additional note to the finished message.
# --------------------------------------------------------------------------------------------

# WARNING -- This cannot make use of any cartulary vars or standard functions
#		as they may not be available at execution time!

function usage
{

	echo "usage: $argv0 [-A agent-host:port] [-a attrs] [-b bcastnet | -B bcastnet] [-c cluster] [-f]"
	echo "	[-i] [-l loggerport] [-m mcast-addr] [-N] [-M mtpt-dir] [-n nnsd-host-list] "
	echo "  [-R root-path | -r root-path] [-S service...] [-s] [-t ningaui-tmp-dir]"
	echo "   [-u prod-id | -U prod-id]"

	cat <<endKat 


The following options may be supplied on the command line.  If all  of the options 
are supplied, it should not be necessary to prompt for additional information. Using
the -F (force) option, causes all defaults to be taken and no prompts to be issued; 
this is not recommended. 

	-A host:port	The multicast agent that ng_narbalek should use if this node is not 
			able to multicast to the cluster (sattelites only). Port may be a ningaui 
			pinkpages varialbe (e.g. NG_PARROT_PORT) and should be supplied with an 
			escaped dollarsign dereference symbol. The host is generally the cluster 
			name (e.g. numbat, or kowari) rather than a specific node name.
			This is needed only when the narbalek name service daemon (nnsd) is NOT
			being used. 

	-a attrs	Node attributes such as Dog or Satellite (enter as space separated list if
			more than one, or multiple -a options can be supplied.)

	-b address	The network portion of the IP address assigned to the interface that should 
			be used for broadcasting.  This will be something like 135.207.  If this 
			option is not supplied, and the force (-F) option is used, the default 
			value set for the cluster (pinkpages) will be used. 

	-B address	Same as -b, except the implication is that the broadcast address applies only
			to the node and not to the cluster (use for Satellite nodes).

	-c cluster	The cluster to join.  NG_CLUSTER used as the default if this is not supplied.
			For safety, this should always be supplied.

	-d		Disable certain automaticly scheduled things from running. Namely ng_rota
			and ng_flist_add, but the list might include others.  This should be used 
			only if the node is not to participate on the cluster immediately.

	-f 		Indicates that all ningaui filesystems have been created and are mounted.  
			Basicly these are any replication manager filesystems (NG_ROOT/fs*), 
			NG_ROOT/site, and NG_ROOT/tmp. 

	-F		Force mode.  No prompts are issued and the default values for any information 
			not supplied on the command line is used.  Be careful and check the 
			/tmp/ng_setup.log file after execution for verification.

	-l port		Logger port.  Defaults to 29010 if not supplied. Must be supplied if multiple 
			clusters share the same broadcast space on the ethernet segment.

	-L league	The narbalek league. Narbalek organises into a communications tree with other
			nodes which belong to the same league.  If not supplied, the default value 
			coded with in narbalek (currently flock) will be used. 

	-m addr		Narbalek should use multicast to establish a communications tree.  Addr is the 
			multicast group IP address that should be used. This is needed only if the 
			narbalek name service daemon is NOT being used. 

	-M dir		The directory where system local mount point directories for things like ng_site,
		 	ng_rmfsXXXX filesystems are kept. If not supplied, then the default is 
			assumed to be /l. 

	-N		No execution mode.  Sets up and verifies things, but does no real damage.

	-n list		Supplies a list of nodes that are running the narbalek name service daemon 
			(ng_nnsd).  This list will be added to the minimal cartulary and will cause 
			ng_narbalek to be started in nnsd mode rather than multicast mode.  Using
			ng_nnsd is the preferred method for narbalek to discover its peers.

	-R path
	-r path		Path to use as NG_ROOT.  
			The path is assumed to apply to all nodes on the cluster if -R is used.  If 
			-r is used, then the root is assumed to be local to the node being setup.

	-s		Skips running ng_site_prep to set up directories. Not advised, but may be 
			necessary if installing on a host that will not become a real node. The 
			filesystem $NG_ROOT/site must exist to run ng_site_prep. 

	-S service	Services to define for the node that are not managed by the service manager
			daemon.  

	-t dir		Directory used as TMP (usually $NG_ROOT/tmp). It is recommended that the 
			TMP directory be a separate filesystem which is created and mounted as
			NG_ROOT/tmp.  

	-U prod-id
	-u prod-id	Sets a production user id that is different than ningaui. If -U is used, the 
			id applies to all of the cluster (still must be supplied on each setup command
			line).  If -u is used, it applies only to the local node. 
			If not supplied, ningaui is assumed (no prompt for this information).

	-v		Verbose mode. Normally, when the -F option is used, informational messages
			are not written to the standard error device. If -F is used, and the 
			informational (progress) messages are deisred, then this option must be 
			used.  If the force option (-F) is not used, this option has no effect. 
			Regardless of the presence of this option, informational mssages, as well
			as prompt/response information, is written to the log /tmp/ng_setup.log.

endKat
}

# create a default cartulary from cart.min, site/cart.min and things set here.
function mk_cartulary
{
	CARTULARY=$NG_ROOT/cartulary

	if [[ -f $CARTULARY  && ! -f $CARTULARY.archive ]]
	then
		move $CARTULARY $CARTULARY.archive
		info_msg "original cartulary moved to $CARTULARY.archive"
	fi
	
	cartf=$CARTULARY
	cat <<endKat >$cartf

# ----------- initial cartulary created by ng_setup -------------------------
#
export NG_ROOT=$NG_ROOT
export NG_CLUSTER=$NG_CLUSTER
export TMP=$TMP
#
# ------------ from cartulary.min -------------------------------------------
#
endKat
	if ! cat $NG_ROOT/lib/cartulary.min >>$cartf
	then
		abort "could not find minimal cartulary in $NG_ROOT/lib"
		cleanup 1
	fi

	echo "# --------------------- from site/cartulary.min ----------------------------" >>$cartf
	cat $site_cart_min >>$cartf

	cp $site_cart_min $NG_ROOT/site/cartulary.min

	echo "# --------------------- special for ng_setup duration ----------------------" >>$cartf
	echo "LOG_ACK_NEEDED=0" >>$cartf	# we dont need acks during setup

	info_msg "temporary cartulary created: $NG_ROOT/cartulary"
}


# look through ifconfig output and spit out one of the net addresses to 
# use as a default when setting the bcast interface.  The guess is probably
# not a good one as if there is more than one net, it is hard to say which 
# is the net that all of the cluster uses. 
function get_net_addrs
{
        ifconfig -a| $awk  '
                /LOOPBACK/      { next; }

                /inet6/         { next; }

                /inet/ {
			gsub( ".*:", "", $2 );                  # bloody linux puts out addr:xxx.xxx.xxx.xxx
                        if( $2+0 == 127 )
                                next;
	
			split( $2, a, "." );
			if( $2+0 >= 192 )
				need = 3;
			else
				if( $2+0 > 128 )
					need = 2;
				else
					need = 1;
			
			for( i = 0; i < need; i ++ )
				addr = addr a[i+1] ".";

			gsub( "[.]$", "", addr );
			addr = addr " ";
                }

		END {
			print addr;
		}
        ' 
}


# get_info --
#	get_info "value|none|null|use-defaut" "prompt string" "default value"
#
#	$1 - value to check	This value is returned if not empty and != "none"
#	$2 - prompt to issue if $1 is empty
#	$3 - default value for prompt. if "", then no default and user must enter something
#		default may be none to allow user to enter an empty response to the prompt.
#
#	if $1 == "none", then the default value is returned (no prompt).
#	prompt type is set to "" or "none", "" allows prompting if $1 is null, none forces use of default
#	ex: get_info ${ng_root:-$prompt_type} "NG_ROOT" "/ningaui"
function get_info
{
	typeset x=""

	if [[ -n $1 ]]			# prompt type 
	then
		case ${1:-missing} in 
			use-default)				# forced to use default (-F on command line sets this)
				if [[ $3 == "empty" || $3 == "blank" ]]		# blank default
				then
					echo ""
					#info_msg "$2" "defaulted to: <blank>"
					log_response -s "$2" "defaulted to: <blank>"
				else
					echo "$3"; 
					#info_msg "$2" "defaulted to: $3"
					log_response -s "$2" "defaulted to: $3"
				fi
				return
				;;

			none)	echo "$3";					# intentionally left blank (none/empty) so we 
				#info_msg "$2" "defaulted to: $3"; 
				log_response -s "$2" "defaulted to: $3"; 
				return
				;;		

			empty)	echo "$3"; 					# just return default
				#info_msg "$2" "defaulted to: $3"; 
				log_response -s "$2" "defaulted to: $3"; 
				return
				;;

			prompt4) ;;				# force a prompt

			missing) echo "*** INTERNAL ERROR *** value missing when trying to get info for: $2" >&2;;

			*)	echo "${1:-$3}"; log_response -s "$2" "command line: ${1:-$3}"; return;;	# value supplied on command line; no prompt
		esac
	fi


	while true
	do
		printf "ENTER: $2 [${3:-no default}]: " >&2
		read x 
		if [[ -n $x ]]
		then
			if [[ $x == "empty" || $x == "blank" ]]
			then 
				echo ""
				log_response "$2" "<blank>"
			else
				echo $x
				log_response "$2" "$x"
			fi
			return
		else
			if [[ -n $3 ]]
			then
				if [[ $3 == "empty"  || $3 == "blank" ]]
				then
					echo ""
					log_response "$2" "default taken: <blank>"
				else
					echo $3
					log_response "$2" "default taken: $3"
				fi
				return
			else
				printf "OOPS! there is no default; you must enter a value or the word 'blank' if you want to supply a blank value\n" >&2
			fi
		fi
	done
}

# adds a command to  a script file that initialises local pinkpages vars for the node
# all local values go to site/cart.min 
function pp_local
{
	echo "ng_ppset  -l $@" >>$pp_init_script

	if (( $forreal < 1 ))
	then
		info_msg "(-N mode) did not  add $@ to $site_cart_min"
		return
	fi

	echo "export ${@/=/=\"}\" 	#ng_setup $(date)" >>$site_cart_min
}

# adds a command to  a script file that initialises cluster space pinkpages vars for the node
# if -m set, then save the value in the site/cartulary.min file too.  There are some cluster 
# values that we might need in this manner. 
function pp_cluster
{
	typeset save2min=0

	if [[ $1 == "-m" ]]
	then
		save2min=1
		shift
	fi
	
	echo "ng_ppset  $@" >>$pp_init_script
	if (( $forreal < 1 ))
	then
		info_msg "(-N mode) did not  add $@ to $site_cart_min"
		return
	fi

	if (( $save2min > 0 ))
	then
		#echo "export $@	#ng_setup $(date)" >>$site_cart_min
		echo "export ${@/=/=\"}\" 	#ng_setup $(date)" >>$site_cart_min
	fi
}

# look at running narbalek and find missing cluster default pp variables. generate the 
# ppset commands to set them. the commands are added to the init script and executed
# enmass.  
function set_default_pp
{
	cat <<'endKat' >/tmp/ng_setup.defppvars
	TMP='$NG_ROOT/tmp'		# this is cluster, a local val may also be set by the script
	PATH='$NG_ROOT/bin:$NG_ROOT/common/bin:$NG_ROOT/common/ast/bin:/usr/common/bin:/usr/local/bin:/bin:/usr/bin:/usr/X11R6/bin:/usr/sbin:/sbin:.'
	DYLD_LIBRARY_PATH='.:/usr/common/ast/lib:/usr/local/lib'
	LD_LIBRARY_PATH='.:/usr/common/ast/lib:/usr/local/lib'
	RUBYLIB='$NG_ROOT/lib'

	TMPDIR='$NG_ROOT/tmp'
	NG_FTP='$NG_ROOT/tmp/ftp'

	NG_CARTULARY='$NG_ROOT/cartulary'

	NG_WOOMERA_PORT='29000'
	NG_RMBT_PORT='29002'
	NG_SENESCHAL_PORT='29004'
	NG_PARROT_PORT='29005'
	NG_CATMAN_PORT='29006'
	NG_NAWAB_PORT='29007'
	NG_SHUNT_PORT='29008'
	NG_EQUERRY_PORT='29009'
	NG_SRVM_PORT='29038'
	NG_REPMGRS_PORT='29050'
	NG_TAPED_PORT='29051'
	NG_RMDB_PORT='29053'
	NG_SPYG_PORT='29054'
	NG_NARBALEK_PORT='29055'
	NG_NNSD_PORT='29056'
	NG_D1AGENT_PORT='29057'
	NG_SENEFUNNEL_PORT='29058'

	NG_RM_DB='$NG_ROOT/site/rm/db'
	NG_RM_LOG='$NG_ROOT/site/log/rm'
	NG_RM_NATTR_OK='1'
	NG_RM_NODES='$NG_ROOT/site/rm/nodes'
	NG_RM_STATS='/dev/null'
	RM_ARENA_ADM='$NG_ROOT/site/arena'
	RM_DUMP='$NG_ROOT/site/rm/dump'
	RM_FS_FULL='500'
	RM_NODE_LEASE='15'
	RM_LARGENREPS='4'
	RM_NREPS='2'
	RM_READY='440'
	RM_SLDMAX='30'

	LOGGER_ADJUNCT='$NG_ROOT/site/log/master.frag'
	LOG_DAEMON='ng_logger:udp'
	LOG_IF_NAME='eth0'
	LOG_ACK_NEEDED='3'
	LOG_NACKLOG='$NG_ROOT/site/log/nack.log'
	LOG_TIMEOUT='3'
	MASTER_LOG='$NG_ROOT/site/log/master'

	XFERLOG='$NG_ROOT/site/log/xferlog'

	LANG="C"
endKat

	ng_nar_get >/tmp/ng_setup.nar_dump
	if [[ ! -s /tmp/ng_setup.nar_dump ]]
	then
		info_msg "no data was found in narbalek; if there should have been, you have 5seconds to abort with a CTL-C"
		sleep 7
	fi

	ng_nar_get | gre "^pinkpages/$NG_CLUSTER:" | sed "s!^pinkpages/$NG_CLUSTER:!!" |$awk \
	-v lf=/tmp/ng_setup.defppvars \
	-v forreal=${forreal:-0} \
	-v verbose=$verbose \
	-v info_log=$info_log \
	'
		{
			split( $1, a, "=" );
			found[a[1]] = $0;
		}

		END {
			while( (getline<lf) > 0 )
			{
				split( $1, a, "=" );
				if( !found[a[1]]   )
				{
					if( forreal )
					{
						if( NF > 0 )				# skip blank lines
						{
							printf( "ng_ppset %s\n", $0 );
							printf( "[INFO]\tdefault pp var not defined in narbalek will be added: %s\n", $0 ) >>info_log;
						}
					}
					else
						printf( "[INFO]\t(-N set) default pp var not defined in narbalek would have added: %s\n", $0 ) >>info_log;
				}
				else
					printf( "[INFO]\tpp var already defined in narbalek; not changed: %s\n", found[a[1]] ) >>info_log;
			}
			close( lf );
			close( info_log );
		}
	'
}

function move
{
	if (( $forreal < 1 ))
	then
		info_msg "(-N mode) did not move $1 to $2"
		return
	fi

	if ! mv $1 $2
	then
		abort "unable to move $1 --> $2"
		cleanup 1
	fi
}

# ensure that any filesystems mounted in system_local_mntd (usually /l) are referenced by symlinks in NG_ROOT
function ensure_m
{
        typeset d="" 
        typeset lname=""

        for d in $(ls -d ${system_local_mntd}/ng_* 2>/dev/null)
        do
                lname=${d#*/ng_}
                if [[ -L $NG_ROOT/$lname ]]             # expected and this is ok
                then
                        info_msg "ensure_m: $NG_ROOT/$lname already exists: $(ls -dl $NG_ROOT/$lname) [OK]"
                else
                        if [[ -d $NG_ROOT/$lname ]]
                        then
                                rmdir $NG_ROOT/$lname 2>/dev/null        # need to remove if it is empty to allow for simple conversion from old style
                        fi

                        if [[ ! -e $NG_ROOT/$lname ]]                   # if not there, then make a link back to the local mount point
                        then
                                if ln -s $d $NG_ROOT/$lname
                                then
                                        info_msg "ensure_m: created link: $NG_ROOT/$lname -> $d  [OK]"
                                else
                                        info_msg "ensure_m: unable to create link: $NG_ROOT/$lname -> $d  [FAIL]"
                                fi
                        else
                                msg="old style mountpoint under NG_ROOT exists: $NG_ROOT/$lname"
                                info_msg "$msg [WARN]"
                        fi
                fi
        done
}

# ensure that a directory ( or -f file) exists; make/touch if not there
function ensure
{
	if (( $forreal < 1 ))
	then
		#info_msg "(-N mode) did not test/create $@"
		return
	fi

	if [[ $1 == "-f" ]]			# ensure file; touch if not there
	then
		shift
		ensure ${1%/*}			# ensure the path exists first

		if [[ ! -f $1 ]]
		then
			info_msg "ensure: making file: $1"
			if ! touch $1
			then
				abort "ensure: unable to create file: $1"
				cleanup 1
			fi
		fi
		return
	fi

	if [[ ! -d $1 ]]
	then
		if [[ -e $1 ]]
		then
			info_msg "ensure: existed but not a directory! $1"
			return
		fi

		info_msg "ensure: making directory: $1"

		if ! mkdir -p $1
		then
			abort "ensure: unable to create directory: $1"
			cleanup 1
		fi
	fi
}


# try a command and abort if it fails
function try
{
	typeset redirect=""
	if [[ $1 == "-o" ]]
	then
		redirect=">$2 2>&1"
		shift 2
	fi

	eval \"\$@\" $redirect
	rc=$?
	if (( $rc != 0 ))
	then
		abort "unable to successfully execute: $@ (rc=$rc)"
		cleanup 1
	fi
}

#	run setup_common and things to make links to ast or other things
function set_links
{
	if (( $forreal < 1 ))
	then
		#info_msg "(-N mode) did not setup common links"
		return
	fi

	if [[ ! -e $NG_ROOT/bin/$preferred_shell ]]			# if run a second time; we can skip
	then
		for x in /usr/common/ast/bin /usr/local/bin /usr/bin /bin
		do
			if [[ -x $x/$preferred_shell ]]
			then
				ln -s $x/$preferred_shell $NG_ROOT/bin/$preferred_shell 
				info_msg "link to preferred shell set: $NG_ROOT/bin/$preferred_shell -> $x/$preferred_shell"
				break;
			fi
		done
	fi

	if ! ksh $NG_ROOT/bin/ng_setup_common >/tmp/ng_setup.common 2>&1				# establish links from $NG_ROOT/common to /usr/common
	then
		cat /tmp/ng_setup.common
		issue_warning "unable to create links or directories in $NG_ROOT/common; correct problems and run ng_setup_common by hand"
	fi
	
	
	
	if [[ ! -e $NG_ROOT/bin/$preferred_shell ]]
	then
		abort "unable to create $NG_ROOT/bin/$preferred_shell symlink"
		cleanup 2
	fi
}

# set random tumbler data for the requested process; narbalek must be up
function set_tumbler
{
	typeset astart=0
	typeset bstart=0

	case $1 in 
		repmgr)						# likely not needed as rm_start sets up correctly now
			astart=$(( $RANDOM % 16 ))			# gen random start so if we break things we have smaller collision chance
			bstart=$(( $RANDOM % 200 ))
			ng_ppset NG_RM_TUMBLER="rm,cpt,$astart,16,$bstart,200"	# repmgr cannot start without this
			;;

		rmdump)
			astart=$(( $RANDOM % 30 ))			# gen random start so if we break things we have smaller collision chance
			bstart=$(( $RANDOM % 24 ))
			ng_ppset NG_RM_DUMP_TUMBLER="rmdump,cpt,$astart,30,$bstart,24"	
			;;

		*)	;;
			
	esac
}

# set the ppvars necessary for the default service manager services. We do NOT turn on netif by 
# default; that must be done manually because the dns name must be manually set too.
# this function is driven only when we believe we are the first node installed. 
function set_services
{
	typeset s=""
	pp_cluster NG_SRVM_LIST="Jobber!Alerter!Stats!Steward!D1agent"		# netif is not started by default, but its info is set below
	for s in jobber alerter steward d1agent netif stats tpchanger
	do
		if (( $forreal > 0 ))
		then
			ng_srv_$s init					# these services provide an initialisation function
		else
			info_msg "(-N mode) did not initialise service: $s"
		fi
	done
}

# abort if file or directory does not exist
function verify
{
	while [[ -n $1 ]]
	do
		if  [[ -e $1 ]]
		then
			echo "verify: file/dir exists: $1"
		else
			abort "cannot find file/dir: $1"
			cleanup 1
		fi
		shift
	done
}

# change to directory and abort if we cannot
function switchto
{
	if ! cd $@
	then
		abort "unable to switch to directory: $@"
		cleanup 1
	fi
}

# ask the user a question.  takes default if in force mode. sets the global var answer
# $1 = prompt  $2 = default
function pregunta
{
	typeset dval=""
	answer=""			# this IS global

	if (( $force > 0 ))
	then
		info_msg "force: default/cmdline value: $1 $2"
		printf "[SUPPRESSED] %s\n[RESPONSE] default/cmdline used, force: %s\n" "$1" "$2" >>$info_log
		answer="$2"
		return;
	fi

	while [[ -z $answer ]]
	do
		if [[ -n $2 ]]
		then
			dval="[$2]"
		fi
		printf "$1 $dval"
		read answer
		if [[ -z $answer ]]
		then
			answer="$2"		# default if provided 
		fi
	done

	printf "[PROMPTED] %s\n[RESPONSE] %s\n" "$1 $dval" "$answer" >>$info_log
}

# check answer against parm and quit if they match $2 is given as the abort reason
function abortif
{
	if [[ $answer == $1 ]]
	then
		if [[ -n $2 ]]
		then
			printf "\t[ABORT] $2\n"
			printf "[ABORT] $@\n" >>$info_log
		fi

		rm -f /tmp/*.$$
		cleanup 1
	fi
}

# terminate and ditch tmp files
function cleanup
{
	rm -f /tmp/*.$$
	exit ${1:-0}
}

# aborts if an expected cartulary variable is not set
function test_cart_vars
{
	typeset v=""
	typeset err=0

	while [[ -n $1 ]]
	do
		eval v="\$$1"
		if [[ -z $v ]]
		then
			abort "required variable not found in $NG_ROOT/cartulary: $1"
			err=$(( $err + 1 ))
		fi		

		shift
	done

	if (( $err > 0 ))
	then
		abort "one or more variables not found in cartulary.min"
		cleanup 2
	fi
}

# set up a narbalek agent file (sattellite nodes and only if using mcast on the rest of the cluster)
function create_nar_agent
{
	if (( $forreal < 1 ))
	then
		info_msg "(-N mode) did not create agent file: $NG_ROOT/site/narbalek/agent"
		return
	fi

	info_msg "creating narbalek agent file $NG_ROOT/site/narbalek/agent"
	eval nar_agent=\"-a $agent\"				# set up for narbalek start
	cat <<endKat >$NG_ROOT/site/narbalek/agent
$agent
# narbalek on this host will use the above agent (usually ng_parrot)
# for multicast communications.  Remove this file completely to 
# disable.  
# File crated by $argv0 on $(date)
endKat
}

# if mcast or nnsd selected we trash the agent file
function ditch_nar_agent
{
	if [[ -f $NG_ROOT/site/narbalek/agent ]]
	then
		info_msg "narbalek agent file removed; not needed when using mcast or nnsd"
		rm -f $NG_ROOT/site/narbalek/agent 
	fi
}

# logging functions -----------------------------------------------------------
function abort
{
	printf "\t[ABORT] $@\n" >&2
	printf "[ABORT] $@\n" >>$info_log
	if (( $warnings > 0 ))
	then
		printf "\n\t[INFO] there were $warnings warning messages issued; see $warn_log for summary\n"
	fi
	cleanup 1
	exit 1
}

function issue_warning
{
	printf "\t[WARN] $@\n" >&2
	echo "[WARN]	$@" >>$warn_log
	echo "[WARN]	$@" >>$info_log
	warnings=$(( $warnings + 1 ))
}

function issue_note
{
	printf "\t[NOTE] $@\n" >&2
	echo "[NOTE]	$@" >>$warn_log
	echo "[NOTE]	$@" >>$info_log
	notes=$(( $notes + 1 ))
}

function info_msg
{
	if (( $force < 1 || $verbose > 0 ))
	then
		printf "\t[INFO] $@\n"  >&2
	fi
	printf "[INFO]	$@\n"  >>$info_log
}

function info_log
{
	printf "[INFO]	$@\n"  >>$info_log
}

function log_response
{
	typeset ptype=""

	if [[ $1 == "-s" ]]
	then
		ptype="[SUPPRESSED]"
		shift
	else
		ptype="[PROMPTED] "
	fi
	
	printf "%s %s\n[RESPONSE]  %s\n" "$ptype" "$1" "$2" >>$info_log
}


# --------------------------------------------------------------------------------------------------------------------
# var assignments must be before the awk version check so that log names are defined
trap cleanup EXIT 1 2 3 15

argv0=${0##*/}
warnings=0			# number of warnings we issued 
notes=0				# number of notes issued
verbose=0			# we issue info msgs to tty only in verbose mode; they are always logged
warn_log=/tmp/ng_setup.warnings
info_log=/tmp/ng_setup.log

system_local_mntd=/l

site_cart_min=/tmp/ng_setup.site_cart_min.$$	# site specific minimal cartulary stuff
pp_init_script=/tmp/ng_setup.pp_init
prompt_type=prompt4	# forces a prompt if we do not have the needed info; -F changes to use-default to supress all prompts

do_site_prep=1 		# causes us to run site-prep 
force=0

cluster_root=""		# is the root the same for all in cluster (if -F, default will be no)
fsok=0			# all filesystems were verified as being setup by user (if -F default is 0 unless -f also supplied)
logger_port=""		

ng_root=""		# set to empty causes get_info to use the default
cluster=""		# force prompt to setup cluster name if not overridden on command line, or in prev setup file
bcast_net=""		# user supplied value
local_bcast_net=""	# -B turns on; save value as a local pp var not cluster
services=""		# allows filer to be set from the command line so we set srv_Filer without an extra step for the user.
prod_id="ningaui"	# allow -u to alter the users choice for prod id (supressesses error msg)
local_prod_id=0		# if set, the id is local to this node

			# narbalek things can be overridden on the command line
agent=""		# if not overridden on the command line, then we will not prompt and use "" as the default
nnsd_list=""		# list of nodes running nnsd
mcast=""		# mcast address that should be used
nar_ctype=""		# type of narbalek discovery (mcast, agent, nnsd) that narbalek will use (if -F default to nnsd unless -A or -m supplied)
nar_league=""		# narbalek league this node belongs to

forreal=1
disable_things=0	# disable rota and flist-add if set (-d)
initial_node=0		# -i sets to indicate this is the first node on the cluster
capture_tmp=0		# normally we dont capture TMP in cart.min; if -t is used we set this and do

preferred_shell=ksh

rm -f /tmp/ng_setup.*	# clean start
printf "[INFO] started: $(date)\n"  >$info_log
>$pp_init_script


# ---------------- test system things like awk differences -------------------------------------------------------
# do this before checking parms etc

if ! gawk ' BEGIN {gsub( "foo.*", "bar", x );} ' >/dev/null 2>&1
then
	issue_warning "gawk is not installed on this node; some ningaui software will not function properly/at all. have systems admin install gawk"

	if ! awk ' BEGIN {gsub( "foo.*", "bar", x );} ' >/dev/null 2>&1				# sun never upgraded awk and thus this fails
	then

        	if ! nawk ' BEGIN {gsub( "foo.*", "bar", x );} ' >/dev/null 2>&1
        	then
                	abort "awk on this node sucks! neither awk nor nawk supports needed funcitons to run setup; install gawk"
        	else
                	info_log "using nawk for setup; have systems admin install gawk"
                	awk=nawk
        	fi
	else
        	info_log "using regular awk for setup; have systems admin install gawk"
        	awk=awk
	fi
else
	info_log "is gawk installed (our preference)?  yes, using it"
	awk=gawk
fi

( set -e; xx=foo:bar; y=${xx//:/ } ) >/dev/null 2>&1
if (( $? != 0 ))
then
	abort "the kshell that is running this script is too old. ningaui requires ksh Version M 1993-12-28 s+ or later"
	cleanup 1				# old shells might not exit from abort call -- be sure!
	exit 1
else
	info_log "is kshell running this ok... yes"
fi


while [[ $1 == -* ]]
do
	case $1 in 
		-A)	nar_ctype=agent; agent="$2"; shift;;
		-a)	def_nattr="$def_nattr$2 "; shift;;
		-b)	bcast_net="$2"; shift;;
		-B)	local_bcast_net=1; bcast_net="$2"; shift;;
		-c)	cluster="$2"; shift;;
		-d)	disable_things=1;;
		-f)	fsok=1;;
		-F)	fsok=1; force=1; prompt_type="use-default";;
		-i)	initial_node=1;;
		-l)	logger_port="$2"; shift;;
		-L)	nar_league="$2"; shift;;
		-m)	nar_ctype=mcast; mcast=$2; shift;;
		-M)	system_local_mntd="$2"; shift;;
		-n)	nar_ctype=nnsd; nnsd_list="$nnsd_list$2 "; shift;;
		-N)	forreal=0;;
		-p)	preferred_shell=$2; shift;;
		-r)	cluster_root=no; ng_root="$2"; shift;;
		-R)	cluster_root=yes; ng_root="$2"; shift;;
		-s)	do_site_prep=0;;
		-S)	services="$services$2 "; shift;;
		-t)	TMP=$2; capture_tmp=1; shift;;
		-u)	prod_id=$2; local_prod_id=1; shift;;
		-U)	prod_id=$2; local_prod_id=0; shift;;
		-v)	verbose=1;;
		

		*)	echo "unknown flag: $1"
			usage
			cleanup 1
			;;
	esac

	shift
done


id -un 2>/dev/null |read id
if [[ -z $id ]]                         # bloody sun does not support -un
then
        id | read id
        id=${id#*\(}
        id=${id%%\)*}
fi

if (( $force < 1 )) && [[ $id != $prod_id ]]
then
	info_msg "current production id is set to $prod_id, use -U or -u option to override this default"
	pregunta "CAUTION: you should be, but are not, logged in as $prod_id; though dangerous, continue as $id anyway?" "n"
	abortif "N*|n*" "switch to ningaui and rerun $argv0"
fi

if (( $local_prod_id > 0 ))
then
	pp_local "NG_PROD_ID=$prod_id"		# set just for the node	
else
	pp_cluster -m "NG_PROD_ID=$prod_id"	# value applies to the cluster and must be in cart.min
fi

if (( $fsok < 1  ))
then
	pregunta "VERIFY: have all necessary repmgr and potoroo filesystems been created and are mounted?" "n"
	abortif "n*" "create the necessary filesystems, then rerun $argv0"
fi

NG_ROOT="$(get_info "${ng_root:-$prompt_type}" "value for NG_ROOT" "/ningaui")"
answer="$( get_info "${cluster_root:-$prompt_type}" "does this value of  NG_ROOT ($NG_ROOT) apply to the whole cluster?" "y" )"
case $answer in 
	Y|y|yes|Yes)	pp_cluster "NG_ROOT=$NG_ROOT";;		# set for whole cluster
	*)		pp_local "NG_ROOT=$NG_ROOT";;		# set just for the node
esac

NG_CLUSTER=""
msg="cluster name"
while [[ $NG_CLUSTER == "" ]]
do
	NG_CLUSTER="$(get_info "${cluster:-$prompt_type}" "$msg" "$NG_CLUSTER")"
	msg="cluster name (blank not allowed)"
done
pp_local "NG_CLUSTER=$NG_CLUSTER"

if (( $force < 1 ))		# when in force mode the default is n, but they can set -S Filer if needed
then
	pregunta "VERIFY: will this node be the Filer?" "n"
	if [[ $answer == "y"* || $answer == "Y"* ]]
	then
		services="${services}Filer "		# add filer to the list
	fi
fi

if [[ -z $NG_ROOT || -z $NG_CLUSTER ]]
then
	echo "either NG_ROOT ($NG_ROOT) or NG_CLUSTER ($NG_CLUSTER) resulted in  empty values; both must be defined or supplied on command line"
	cleanup 1
fi

info_msg "NG_ROOT=$NG_ROOT;  NG_CLUSTER=$NG_CLUSTER"

export NG_ROOT
export NG_CLUSTER

if [[ ! -d $NG_ROOT ]]
then
	abort "cannot find NG_ROOT ($NG_ROOT) directory"
	cleanup 1
else
	info_msg "found NG_ROOT directory ($NG_ROOT)"
fi


if [[ ! -d $NG_ROOT/bin || ! -d $NG_ROOT/lib ]]
then
	abort "$NG_ROOT/bin and/or $NG_ROOT/lib are missing; please install ningaui into $NG_ROOT before running $argv0"
	cleanup 1 
fi


#if [[ -f $NG_ROOT/lib/cartulary.min ]]
#then
#	cp $NG_ROOT/lib/cartulary.min $NG_ROOT/cartulary		# prevent problems with ppget on some os types
#else
#	touch $NG_ROOT/cartulary
#fi


if [[ -d $system_local_mntd ]]
then
	ensure_m			# ensure that things are referenced from NG_ROOT
else
	issue_warning "did not find system local mount directory: $system_local_mntd"
fi

# must ensure a few things exist before we run site_prep where the rest will be taken care of
TMP=${TMP:-$NG_ROOT/tmp}
ensure $TMP			# they said they are all there; we must make if not and they may get screwed if not a filesysem
ensure $NG_ROOT/site
ensure $NG_ROOT/site/narbalek			
ensure $NG_ROOT/site/rm					
ensure $NG_ROOT/site/log

ensure -f $NG_ROOT/site/rm/rm_dbd.ckpt		# this causes site_prep to link ng_rm_db to ng_rm_dbd, and allows node to start dbd 

if (( $capture_tmp > 0 ))			# user supplied, otherwise we default to NG_ROOT/tmp as is the cluster default 
then
	pp_local "TMP=$TMP"
fi


PATH="$NG_ROOT/bin:$NG_ROOT/common/ast/bin:$PATH"
if [[ -d /sbin ]]
then
	PATH=$PATH:/sbin
fi

this_node="$( ng_sysname -s )"
set_links			# set links in NG_ROOT ($preferred_shell) and ROOT/common 


NG_DEF_NATTR="$( get_info "${def_nattr:-$prompt_type}" "node attribute(s) (e.g. Satellite)" "blank" )"
pp_local NG_DEF_NATTR="$NG_DEF_NATTR"
if [[ $NG_DEF_NATTR == *Satellite* ]]
then
	pp_local LOG_ACK_NEEDED=1		# on satellite process will only ever get one ack, keeps it from expecting more
	pp_local NG_LOG_BCAST=localhost 	# causes ng_log() to send to ng_logd on this node and not to bcast 
	pp_local NG_LOGGER_PORT=29097		# satellite nodes cannot use the same cluster port in case they are on the same bcast net
	pp_local NG_SHUNT_C2M=1			# force firewall connection mode for shunt data port establishment
else
	if (( $force < 1 ))			# if not Satellite, then we need a real logger port for the cluster
	then
		echo "CAUTION: the ng_logger well known port must be unique for the cluster when multiple clusters share the same broadcast network."
		echo "CAUTION: if other nodes are already running, ensure that the value you supply matches their value"
	fi
	logger_port="$(get_info "${logger_port:-$prompt_type}" "cluster wide ng_logger port" "29010" )"
	pp_cluster -m "NG_LOGGER_PORT=$logger_port"
fi

info_msg "network address available for broadcast: $(get_net_addrs)"
NG_BROADCAST_NET="$(get_info "${bcast_net:-$prompt_type}" "broadcast newtwork (e.g. 135.207)" "" )"
answer="$( get_info "${local_bcast_net:-$prompt_type}" "does this value  ($NG_BROADCAST_NET) apply to the whole cluster?" "y" )"
if [[ -n $NG_BROADCAST_NET ]]
then
	if [[ $answer == "no" || $answer == "N"* ]] 		# this address just this node, register in min and add a local pp var
	then
		pp_local NG_BROADCAST_NET="$NG_BROADCAST_NET"		 
	else
		pp_cluster -m NG_BROADCAST_NET="$NG_BROADCAST_NET"			# set it for the cluster
	fi
fi

need_nnsd=0
nar_ctype="$( get_info "${nar_ctype:-$prompt_type}" "narbalek discovery method (nnsd, mcast, agent) " "nnsd" )"
while [[ -n $nar_ctype ]]
do
	case $nar_ctype in
		mcast)
			mcast="$( get_info "$mcast" "multicast group address for narbalek discovery" "" )"	# no default -- must supply via opt if using -F
			nar_ctype=
			ditch_nar_agent			# need to ensure agent file is gone
			;;	

		agent)
			agent="$( get_info "${agent:-$prompt_type}" "host:port of narbalek agent" "$NG_CLUSTER:29005" )"
			create_nar_agent
			nar_ctype=
			;;	

		nnsd)
			nnsd_list="$( get_info "${nnsd_list:-$prompt_type}" "list of node names where ng_nnsd runs" "$this_node" )"
			need_nnsd=1
			nar_ctype=
			ditch_nar_agent			# need to ensure agent file is gone
			if [[ -n $nnsd_list ]]
			then
				pp_cluster -m NG_NNSD_NODES="$nnsd_list"
			fi
			;;	

		*)
			nar_ctype="$( get_info "prompt4" "narbalek discovery method (nnsd, mcast, agent) [nnsd]" "nnsd" )"
			;;
	esac
done
nar_league="$(get_info "${nar_league:-$prompt_type}" "Narbalek league for this node" "flock" )"
if [[ -n $nar_league ]]
then
	pp_local NG_NARBALEK_LEAGUE="$nar_league"
	nar_league="-l $nar_league"
fi

# these are for servcies that are not supplied/managed by service manager
for s in $services
do
	info_msg "adding service: $s"
	pp_cluster "srv_$s=$this_node"
done

if (( $forreal < 1 ))
then
	info_msg "-N supplied on command line, not continuing setup"
	info_msg "NG_ROOT = $NG_ROOT"
	info_msg "NG_CLUSTER = $NG_CLUSTER"
	info_msg "PATH = $PATH"
	info_msg "narbalek info: discovery=$nar_ctype mcast=$mcast agent=$agent nnsdlist=$nnsd_list"
	info_msg "bcast net = $NG_BROADCAST_NET"
	set_services
	cat $pp_init_script
	cleanup 0
fi

mk_cartulary			# make a temp cartulary then
. $cartf			# pick up the stuff
test_cart_vars NG_NARBALEK_PORT 
test_cart_vars NG_NNSD_PORT 

# ==================================================================================================
# ============= do NOT prompt for or assign cartulary vars below here! =============================
# ============= set_default_pp script call is the only exception as it needs narbalek running ======
# ==================================================================================================

if (( $forreal > 0 ))
then
	ksh $NG_ROOT/bin/ng_fix_hashbang -v  $NG_ROOT/bin >/tmp/ng_setup.fixhb 2>&1
fi

info_msg "starting narbalek to setup pinkpages variables for the node/cluster; this may take a few seconds"
( 
	cd $NG_ROOT/site/narbalek; 
	if (( $need_nnsd > 0 ))
	then
		nar_option="-d $NG_NNSD_PORT"

		for x in ${nnsd_list:-$this_node}		# if no list, then start one here 
		do
			if [[ $x == $this_node ]]		# if our node is listed, we must start it in case it is the only one up
			then
				info_msg "starting narbalek name service daemon (ng_nnsd)"
				ng_nnsd -v -v -p ${NG_NNSD_PORT:-29056} >/tmp/ng_setup.nnsd 2>&1 &
				nnsd_pid=$!
				nnsd_running=1
			fi
		done	
	fi
	info_msg "runing: ng_narbalek -c $NG_CLUSTER $nar_league $nar_agent -v $nar_option -p ${NG_NARBALEK_PORT} >/tmp/ng_setup.narbalek 2>&1 &"
	NG_NNSD_NODES="$nnsd_list" ng_narbalek -c $NG_CLUSTER $nar_league $nar_agent -v -v -v $nar_option -p ${NG_NARBALEK_PORT} >/tmp/ng_setup.narbalek 2>&1 & 	
	nar_pid=$!

	echo $nar_pid ${nnsd_running:-0} ${nnsd_pid:-0}
) | read nar_pid nnsd_running nnsd_pid

if (( $nnsd_pid < 1 ))
then
	nnsd_pid=""		# if we did not start it, then we should not attempt to kill it later
fi

info_msg "waiting for narbalek to stabilise"
sleep 3				# some initial time before we start to query its state
timeout=50			# number of times through loop -- about 100s
nar_state=1			# assume good
x=""
while [[ -z "$x" || $x == *ORPHANED* ]]
do
	sleep 2
	ng_nar_get -p $NG_NARBALEK_PORT -t 1 -i |read x		# get a status dump; when ORPHANED goes away, nar is up and parented
	info_msg "narbalek state: $x"
	timeout=$(( $timeout - 1 ))
	if (( $timeout < 1  ))
	then
		nar_state=0
		issue_warning "narbalek seems to be having problems -- giving up waiting on it"
		issue_warning "	local pinkpages variables may not be set right"
		kill -9 $nnsd_pid $nar_pid >/dev/null 2>&1
	fi
done



if (( ${nar_state:-0} > 0 ))			# only if we think narbalek started ok
then
	sleep 1						# ensure narbalek gets a load before we start digging info from it
	info_msg "narbalek seems alive and well: $x"

	info_msg "searching narbalek for cluster default values that already exist"
	set_default_pp >>$pp_init_script	# add any cluster default pp values that are not in narbalek

	# if narbalek seems to be the only one running, then we assume the first in flock and we will bootstrap it
	# with NG_ROOT/lib/narbalek.init -- only if there is not a checkpoint file (meaning narbalek has been started
	# on the node before)
#---------------------------------------------------------------------------------------------------------------------------
# we used to care if narbalek became root; we no do not set any flock vars, so skip this
#	if [[ $x == *ROOT* ]]
#	then
#		if (( $need_nar_bootstrap > 0 ))
#		then
#			info_msg "narbalek has proclaimed itself root; bootstrapping it from $NG_ROOT/lib/narbalek.init"
#			try ng_nar_bootstrap -f $NG_ROOT/lib/narbalek.init >/tmp/ng_setup.nar_bootstrap 2>&1
#		else
#			info_msg "narbalek has proclaimed itself root; checkpoint found, so not bootstrapped"
#		fi
#	fi
#---------------------------------------------------------------------------------------------------------------------------

	if [[ $(ng_ppget -L NG_SRVM_LIST) == "" ]]			# if no service manager stuff for the cluster
	then							# prevent overidding on an existing cluster that is getting new
		set_services					# add service vars to the init script
		fullname=$(ng_sysname -f)
		if [[ $fullname == *"."* ]]			# if sysadmin didn't define host.domain.foo then dont save
		then
			ng_ppset NG_DOMAIN=${fullname#*.}		# needed for spyglass and other mailer oriented things
		else
			issue_warning "ng_sysname -f did not generate a fully qualified domain name; NG_DOMAIN was not set"
		fi
	fi

# tumblers are now all set properly in startup scripts if they are not defined in ppages
#	if [[ $(ng_ppget -L NG_RM_TUMBLER) == "" ]]
#	then
#		#set_tumbler repmgr			# repmgr tumbler not needed as rm_start sets correctly on new cluster
#		set_tumbler rmdump
#	fi


	info_msg "adding pinkpages variables to narbalek data"
	try -o /tmp/ng_setup.pp_init.log $preferred_shell -x $pp_init_script			# run the init script to set cluster and local vars

	ng_nar_get >/tmp/ng_setup.nar_dump 2>&1
	info_msg "captured information from narbalek in /tmp/ng_setup.nar_dump"

	info_msg "stopping narbalek"
	printf 'xit4360\n' |ng_sendtcp -e "#end#" localhost:$NG_NARBALEK_PORT  2>/dev/null
	if (( ${nnsd_running:-0} > 0 ))
	then
		info_msg "stopping nnsd ($NG_NNSD_PORT)"
		printf '0:exit:\n' |ng_sendudp  -r "#end#" -t 2 localhost:${NG_NNSD_PORT:-29056}  2>/dev/null
	fi
fi

if (( do_site_prep > 0 ))
then
	export NG_PROD_ID=$prod_id
	export NG_ROOT
	info_msg "running ng_site_prep to initialise the node (mostly directory creation)"
	info_msg "	(ng_site_prep info is verbose and will be saved in /tmp/ng_setup.prep)"
	try -o /tmp/ng_setup.prep  ng_site_prep 		# build the directories and set ownership
	info_msg "site prep completed"
else
	issue_note "running of ng_site_prep skipped"
fi

if (( $disable_things > 0 ))
then
	issue_note "disabling ng_rota, ng_flist_add with goaway files"
	try ng_goaway ng_rota
	try ng_goaway ng_rm_flist_add
fi


# must fix NG_ROOT in the rc.d script (before sysadmin installs it), and in ng_wrapper
# site-prep now  fixes wrapper and rc.d so that it is right after a refurb and will be right here.
# however, if we dont run site-prep, we must adjust these now.
#
if (( $do_site_prep <= 0 ))
then
	info_msg "adjusting NG_ROOT values in intiailisation/startup scripts"
	sed "s!_ROOT_PATH_!$NG_ROOT!g; s/_PROD_ID_/${NG_PROD_ID:-ningaui}/g" <$NG_ROOT/bin/ng_FIXUP_rc.d >/tmp/ng_rc.d.mod
	cp /tmp/ng_rc.d.mod $NG_ROOT/bin/ng_rc.d		# cannot use mv as it bloody errors off if cannot keep group the same
	rm /tmp/ng_rc.d.mod
fi


info_msg "ng_rc.d was adjusted to set NG_ROOT to $NG_ROOT. ng_rc.d should be installed in /etc/rc.d as needed on the system"
info_msg "a detailed account of this execution of setup has been saved in $info_log"

# ------------- end -- no action below here ---------------------------------------------

if (( $(( $warn + $notes )) > 0 ))
then
	echo "*** NOTE: there were $warnings warnings and $notes notes issued (see $warn_log for summary)"
fi

cat <<endKat

	============================================================================
	Ningaui setup is done. 

	Run ng_node start, or reboot the system after installing ng_rc.d into 
	the proper initialisation location for the node (/etc/rc.d or rc.init).
	Ensure that the environment variable NG_ROOT is exported to your environment
	before executing ng_node. 

endKat

	if (( $disable_things > 0 ))
	then
	cat <<endKat
	Once you are ready to let rota run, and to receive repmgr files, 
	dont forget to remove the goaway files created during setup:
		 ng_goaway -r ng_rota ng_rm_flist_add
endKat
	fi
	cat <<endKat

	If there are specialised pinkpages variables for the node (local values)
	they can be setup from another node, before this node is started, using 
	the -h option on ng_ppset. After starting this node, use ng_ppset with 
	the  -l option on to set variables that apply only to this node.  These 
	are usually related to repmgr filesystem discounts and/or connect direct 
	information. 

	Ng_dosu must have its mode changed to set the suid bit in order for 
	ng_equerry, tape, and the network interface service to function properly.
endKat

rm -f /tmp/*.$$ 
cleanup 0
