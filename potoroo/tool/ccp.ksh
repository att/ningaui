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
# Mnemonic:	ng_ccp -- cluster copy
# Abstract:	Copy files between cluster nodes
#		
# Date:		
# Author:	Mark Plotnick
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN


# figures out the destination filename and echos to caller.  
# the only 'bug' here is that if -d /tmp supplied and only one file on the command line
# we must assume that -d is a full filename and not a directory name.  To get it to 
# work user must suppliy -d /tmp/.  No way round that I can see.
function get_dest
{
	typeset f=""

	if [[ -z $dest ]]			# use a spaceman name if no dest given
	then
		verbose "dest not supplied, copy to random repmgr filesystem"
		ng_rcmd -t 300 $host ng_spaceman_nm $1 | read f
		echo ${f%\~}			# trash trailing ~ from spaceman -- shunt will add protection
	else
		if [[ $dest != /* ]]		# assume its a basename, and allocate a repmgr fs ontop of it
		then
			verbose "dest supplied, not fully qualified, assume its a basename and copy to rmfs/$dest"
			ng_rcmd -t 300 $host ng_spaceman_nm $dest
		else
			if [[ $files2copy -gt 1  ||  $dest = */ ]]		# multiple files on cmd line, we assume dest is a directory
			then
				verbose "dest supplied, qualified, mult files to send or dest has ending /, assume $dest is directory"
				echo "${dest%/}/${1##*/}"			
			else
				if ng_rcmd -t 300 $host "test -d $dest/  >/dev/null 2>&1"	# true if dest is a directory
				then
					verbose "dest supplied, qualified, rcmd indicated $dest is a directory"
					echo "${dest%/}/${1##*/}"			
				else
					verbose "dest supplied, qualified, single file on cmdline, rcmd !dir, assume $dest is target file"
					echo $dest					# assume full name supplied
				fi
			fi
		fi
	fi
}

# ------------------------------------------------------------------


ustring="[-a] [-d dest] [-p shunt-port] [-q] [-t] [-v] <host> file ..."
RCP_CMD=/usr/bin/ng_rcp
cmd_flag=""
port=""
comment=""
dest=""
rmt_tmp=0;			# -t sets to 1 -- cause file to be put into $TMP on remote node
use_rand_net=0			# -r selects network at random

# -------------------------------------------------------------------
while [[ $1 = -* ]]
do
	case $1 in 
		-c)	comment="$2"; shift;;			# identifier passed straight into shunt
		-d)	dest=$2; shift;;
		-p)	port="-p $2"; shift;;
		-r)	use_rand_net=1;;			# if NG_CCP_RANDNETS is set, then we are permitted to use any network to send
		-s)	shunt=1;;				# unnecessary as shunt is used regardless
		-v)	verbose=1; cmd_flag="$cmd_flag-v ";;
		-q)	cmd_flag="$cmd_flag-q ";;			# quiet mode (no log msg)
		-t)	rmt_tmp=1;;				# put file into $TMP on the destination host
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

host=$1
shift			# remaining parms are files to copy to dest
files2copy=$#			# files supplied on command line; if more than 1, then we assume dest is a directory path

if (( $rmt_tmp > 0 ))
then
	ng_rcmd $host ng_wrapper -e TMP | read rtmp junk			# remote nodes TMP value
	if [[ -z $rtmp ]]
	then
		log_msg "cannot get TMP from $host"
		cleanup 1
	fi
	
	if [[ $dest == /* ]]			# if user gave a full path -- strip it all
	then
		dest=$rtmp/${dest##*/}			# strip path when putting in TMP
	else
		dest=$rtmp/${dest}		# else assume that its TMP/path/file allowing for a specifc subdir in tmp
	fi
	verbose "remote tmp dest = $dest"
fi

if ng_test_nattr "Satellite"		# if we are running on a satellite
then
	hname_suffix1=""
else
	hn=$(ng_get_nattr $host)
	if ng_test_nattr -l "$hn " Satellite		# or if the host we are going to is satelite
	then
		hname_suffix1="";
	else
		if [[ $NG_CCP_HSUFFIX == "none" ]]		# pinkpages does not support an empty value, so we allow this to be ""
		then
			hname_suffix1=""
		else
			hname_suffix1="${NG_CCP_HSUFFIX:--g}"	# some clusters dont have this, prevent unnecessary fails if they set to ""
		fi
	fi
fi

if (( $use_rand_net > 0  )) 		# user requested random selection
then
	if [[  -n $NG_CCP_RANDNETS ]]		# and a random list defined
	then
		verbose "transport network will be selected randomly from $NG_CCP_RANDNETS"

		awk -v "r=$RANDOM" -v "choices=$NG_CCP_RANDNETS" '
		BEGIN { 
			x = split( choices, a, ":" ); 
			r = (r % x) +1; 
			printf( "%s ",  a[r] );			# first choice
			if( r >= x ) 
				printf( "%s\n", a[1] ); 
			else
				printf( "%s\n", a[r+1] ); 
		}' |read hname_suffix1 hname_suffix2 junk

		ng_log $PRI_INFO $argv0 "random transport: selected ${host}${hname_suffix1%.*} for ${1##*/}"
	else
		log_msg "random transport network requested, but not enabled; set NG_CCP_RANDNETS to enable on the cluster"
	fi
fi


verbose "hostname suffix try1=$hname_suffix1 try2=$hname_suffix2"

for f in "$@"
do
	if [[ ! -r $f ]]
	then
		log_msg "source file not readable: $f"
		cleanup 2
	fi

	destf=`get_dest $f`		# figure out destfn, get new spaceman for each name on cmd line
	if [[ -z $destf ]]
	then
		log_msg "unable to get a remote filename from $host"
		cleanup 3
	fi
	echo "$destf"

	verbose "$f ==> $host${hname_suffix1%.*} $destf"

	if ! ng_shunt $cmd_flag -c "$comment" $port ${host}${hname_suffix1%.*} $f $destf		# try fast pipe first
	then
		verbose "re-attempting transfer using ${host}"
		if ! ng_shunt $cmd_flag -c "$comment" $port ${host}${hname_suffix2%.*} $f $destf	# redundant but slower 100BaseT
		then
			ng_log $PRI_WARN $argv0 "unable to send file via any interface to host: $host ($destf)"
			log_msg "transfer of $f to $host $destf using ng_shunt failed"
			cleanup 1
		fi

	fi
done

cleanup 0

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_ccp:Ningaui Cluster Copy)

&space
&synop	
	ng_ccp [-r] [-p port] [-q] [-v] host file [file...file]
&break
	ng_ccp -d destdir/  [-r] [-p port] [-q] [-v] host file [file...file]
&break
	ng_ccp -d destdir/filename [-r] [-p port] [-q] [-v] host file 
&break
	ng_ccp -d filename [-p port] [-r] [-q] [-v] host file 

&space
&desc	&This copies the files listed on the command line to the named host optionally
	placing the files into a specified directory or file on the remote host.
	By default, the file is coppied to a directory within the replication manager
	filespace, and is given the same basename as it had on the source node. The 
	destination flag (-d) can be used to further control the filename to which the 
	file is coppied.  
&space
	The destination option can be used to specify:
&space
&beglist
&item	A fully qualified file name (e.g.  /ningaui/tmp/foo.4360)
&item	A directory name (e.g. /ningaui/tmp/)
&item	A new basename (e.g. foo.43086)
&endlist
&space
	When a directory name is supplied (the trailing slant character is manditory) all files
	listed on the command line will be coppied to that directory. If the destination option 
	supplies only a basename, the file is placed into replication manager file space using 
	the destination name (base) rather than the basename of the file supplied on the command 
	line.  When a basename, or fully qualified filaname,  is supplied, only one file may be 
	supplied on the command line. 

&space
	&This will attempt to copy files using the network interface that should provide the 
	fastest path between the source and destination node.  An assumption is made by 
	&this that the if the hostname is appended with &lit(-g) the resulting DNS name 
	will return the highspeed IP address.  If communcations over the highspeed link fail, 
	a second attempt is made using just the hostname when looking up the address.

&space
	When &lit(ng_shunt) is used to copy files, the date and time of the destination 
	file reflects the time of the copy and &stress(NOT) the timestamp of the file
	that was coppied.  This behaviour is different than what was supported by &lit(ng_rcp)
	originally.

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-d dest : 
	Supplies the destination directory path, fully qualified filename or basename as 
	dest.  See the description above for the possible values of dest.
	If omitted, the file is placed randomly into replication manger filespace using 
	the basename of the source. 
&space
&term	-r : Random transport selection.  If the NG_CCP_RANDNETS pinkpages variable is set, then 
	the transport network will be randomly selected from the list.  The variable should contain 
	a colon separated list of hostname suffixes (e.g. -g:.) to be used. The dot (.) suffex is
	translated to no suffix. If the pinkpages variable is not defined, using the -r option has
	no effect. 

.** &space
.** &term	-s : Use ng_shunt rather than ng_rcp (the emerging preference, soon this will be the 
.**	default). If the shunt option is supplied, then the &lit(-q) and &lit(-p) options are 
.**	recognised.

&space
&term	-p port : Supplies the port number that should be used to communicated with &lit(ng_shunt)
	on the remote machine. If not supplied the cartulary value &lit(NG_SHUNT_PORT) is expected
	to contain this value.
&space
&term	-q : Quiet mode. Supresses &ital(ng_shunt) master log messages (not recommended).
&space
&term	-t : Place file into $TMP on the destination host. If the -d path option is also given, 
	the pathname part of the destination is ignored.  Thus, the -d option can be used to 
	give the file a different name in the $TMP directory without the caller having to 
	determine the TMP directory. If the path supplied with the -d option is a relative path
	(no leading slant character), the destination directory will be assumed to be $TMP/path.
&space
&term	-v : Verbose.  Two of these cause &lit(ng_shunt) to generate a detailed session log.
&endterms


&space
&parms	&This expects a minimum of two positional command line parameters.  The first is the name 
	of the host to which to send the file(s).  The remainder of the parameters are the names of 
	the files that should be sent. 

&space
&examp
	The following examples illustrate how to copy the file /tmp/foo to different directories and
	files on a remote node.
&space
&litblkb
    ng_ccp nodex /tmp/foo               # copy into random repmgr dir, keep name
    ng_ccp -d /tmp/ nodex /tmp/foo      # copy and keep same name
    ng_ccp -d /tmp/bar nodex /tmp/foo   # copy and rename file
    ng_ccp -d bar -t nodex /tmp/foo     # rename and put into $TMP on dest host
    ng_ccp -d foo/bar -t nodex /tmp/foo # rename into a subdir under $TMP
&litblke

&space
&exit	A non-zero return code indicates that a file could not be successfully coppied. &This will
	stop with the first failed copy if multiple files are supplied on the command line.

&space
&see	ng_shunt

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	28 Jul 2003 (sd) : Revision to add shunt support and documentation.
&term	16 Jan 2004 (sd) : Changed such that if -d is not supplied, a filename in repmgr
	space is used rather than sending to /ningaui/tmp/ftp.
&term	07 May 2004 (sd) : Corrected examples in man page.
&term	07 Sep 2004 (sd) : Corrected usage string.
&term	11 Oct 2004 (sd) : Added comment capability
&term	15 Jun 2005 (sd) : Warning suppressed for host-g not there unless in verbose mode; Now issues
	a warning to the master log in this case. 
&term	16 Jun 2005 (sd) : Tweeked ng_log messages to indicate retry if -g fails.
&term	21 Jul 2005 (sd) : Destination is now checked to see if it is a directory on the target (trailing 
		slant on the dest name is no longer required to indicate dest is a directory.
&term	07 Aug 2005 (sd) : We trash the trail ~ from spaceman generated name as shunt will add protection.
&term	14 Aug 2005 (sd) : Changed ls -d to test -d as ls on macs returns 0 even if file does not exist. 
&term	09 Sep 2005 (sd) : Checks to see if the source file is readable and errors if it is not.
&term	27 Oct 2005 (sd) : Added -t option to retrieve remote node's &lit($TMP) variable value and use that
		as the destination directory.
&term	21 Aug 2006 (sd) : cleaned up deprecated code fragments still hanging round.
&term	01 Dec 2008 (sd0 : Added filename to warning message issued to master log.
&term	12 Oct 2009 (sd) : Added random transport (-r) support.
&endterms

&scd_end
------------------------------------------------------------------ */
