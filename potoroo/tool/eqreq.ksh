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
# Mnemonic:	eqrew - equerry request
# Abstract:	command line interface to the running equerry daemon. also provides
#		an interface that the equerry daemon uses to  discover things and an interface
# 		for establishing/managing the narbalek vars needed by equerry. It also provides
#		the mount/umount functionality for the daemon so that we can ensure that the 
#		arena list is properly updated and such after filesystems are [un]mounted.
#		The script also provides the detach/attach funcitons that allow a filesystem,
#		and the associated ng_rm_dbd data, to be shifted to another node. 
#		
#		WARNING:
#			both this script and eq_educe.rb parse the gen data input. If
#			the input format changes, eq_deduce.rb must be changed too.
#
# Date:		02 August 2007
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# read a table of fs specifications and generate the eqreq commands that can 
# be executed to do the setup. table format is: 
#fsys-id         label/dev       mount          type    option  src     flags           	host:state
#
#rmfs0210site     .              /l/ng_site      ufs     .       pk21    NOACTION,PRIVATE        roo06:rw
#rmfs0210apps     .              /l/ng_apps      ufs     .       pk21    .              	 roo06:rw
#rmfs0210tmp      .              /l/ng_tmp       ufs     .       pk21    NOACTION,PRIVATE        roo06:rw 
#rmfs0234         .              /l/rmfs0234     ufs     .       pk23    .               	roo06:rw
#rmfs0235         .              /l/rmfs0235     ufs     .       pk23    .               	roo06:rw
#pkfs0236         .              /l/pkfs0236     ufs     .       pk23    .               	roo06:rw
#
#dib99_ng_tmp    /dev/da0s1f     /ningaui/tmp    ufs     .       local   NOACTION,PRIVATE      dib99:rw
#dib99_ng_site   /dev/da0s1e     /l/ng_site      ufs     .       local   NOACTION,PRIVATE      dib99:rw
#dib99_ng_fs00   /dev/da0s1g     /l/ng_fs00      ufs     .       local   PRIVATE               dib99:rw
#dib99_ng_fs02   /dev/twed1s1e   /l/ng_fs02      ufs     .       local   PRIVATE               dib99:rw
#
# if no options are needed (remember rw/ro and noauto are automatically added)
# then use a . as a place holder.  Mtpt is a dot unless NG_ROOT/label is not 
# the target mtpt.  If device is given as just rmfs0241 then /dev/type/name is generated.
#
# the token drop can be used in place of host:mode to indicate that the filesystem should
# be dropped by equerry
#
function gen_nar_cmds
{
	# this version 2.0 of gen; version 1.x did not have a flags col and we are back compat with that
	awk -v verbose=$verbose '
		BEGIN {
			default_mtpt_dir = "no-def-mtptdir-supplied";
			default_device_dir = "";		
			valid_rec = 0;
		}

		/^!/ {
			if( split( $1, a, "=" ) == 2 )
			{
				varname = a[1];
				value = a[2];
			}
			else
			{
				varname = $1;
				value = $2 == "=" ? $3 : $2;
			}

			if( varname == "!default_mtpt_dir" )
				default_mtpt_dir = value;

			if( varname == "!default_device_dir" )
				default_device_dir = value;
		}

		{
			if( substr( $1, 1, 1 ) == "#" || NF < 6 )		# skip comments and old records
				next;


			#disk-id	label		mount		type	option	src	flags	host:state
			x = split( $2, a, "/" );			# if label is /dev/ufs/rmfs0000 get rmfs0000
			if( $1 == "." )
				disk_id = a[x];		# diskid and label can be the same; one, but not both, then can be a .
			else
				disk_id = $1;	

			if( $2 == "." )
				label = $1;
			else
				label = $2;

			if( label == "." )
			{
				printf( "bad record: %s\n", $0 ) >"/dev/fd/2";
				next;
			}

			valid_rec++;

			fsname = a[x];
			mtpt = $3;
			type = $4
			options = $5;
			src = $6;	
			flags = $7;
			if( flags == "." )
				flags = "";
			else
				gsub( ",", " ", flags );			# in nar var these are space separated

			host_start = 8;					# input col where host names start


			if( x == 1 )				# not a /dev/....
			{
				if( default_device_dir == "" )
					dev = label;				# no default, then its just a label
				else
					dev = sprintf( "%s/%s", default_device_dir, label);		# build it into a device name
			}
			else
				dev = label;					# user gave full path, use as is

			x = split( mtpt, a, "/" );			# suss last bit for old style mtpt= 
			umtptd = "";					# build mtpt dir from user string
			for( i = (a[1] == "" ? 2 : 1); i < x; i++ )
				umtptd = umtptd "/" a[i];
			if( umtptd == "" )
				umtptd = default_mtpt_dir; 
			if( x == 1 )	 		# user supplied just ng_site or .; must make full name for mount=
			{
				umtptd = default_mtpt_dir; 
				mtpt = sprintf( "mount=%s/%s mtpt=%s", default_mtpt_dir, mtpt == "." ? fsname : mtpt, umtptd) 	
			}
			else
			{
				if( umtptd != ""  ||  default_mtpt_dir == "" )
					mtpt = sprintf( "mount=%s mtpt=%s", mtpt, umtptd );	#use full name given  /l/ng_site
				else
					mtpt = sprintf( "mount=%s/%s mtpt=%s", default_mtpt_dir, fsname, default_mtpt_dir );
			}

			if( options != "." )
				opts = sprintf( "opts=%s", options );
			else
				opts = "";


			if( verbose )
				printf( "\n#%s\n", $0 );

			if( $(host_start) == "drop" )
				printf( "ng_eqreq drop %s\n", fsname );
			else
			{
				for( i = host_start; i <= NF; i++ )		# see if any node is to really mount it
				{
					if( index( $(i), ":r" ) != 0 )
						break;
				}
	
				# if i is <= NF we found a node that really should mount the disk so we must run set parm 
				# otherwise we assume all are ignorning it and we run set rparm which keeps nar var usage down.
				printf( "ng_eqreq set %s %s \"label=%s %s type=%s src=%s %s %s\"\n", i <= NF ? "parm" : "rparm", disk_id, dev, flags, type, src, mtpt, opts );
		
				printf( "ng_eqreq set host %s ", disk_id );		# generate the host variable 
				for( i = host_start; i <= NF; i++ )
					printf( "%s ", $(i) );
				printf( "\n" );
			}
		}
	' 
} 


# install the new fstab created by equerry.  we assume that we suss out everything before the cookie from the current fstab and append to 
# that the information in $eq_fstab The resulting file is then moved over /etc/fstab bo dosu.
#	#1 is the fstab that equerry has created
#
function install_fstab
{
	typeset new_fstab=$TMP/PID$$.fstab 
	typeset eq_fstab=${1:-no-fstab-given}

	if(( $allow_mounts > 0 ))
	then
		if [[ ! -f $eq_fstab ]]
		then
			log_msg "install_fstab: cannot find new fstab: $eq_fstab"
			return 1
		fi

		if ! sed "/$fstab_cookie/q" <$sys_fstab >$new_fstab		# capture stuff off limits for us to change, including cookie line
		then
			log_msg "install_fstab: cannot suss from existing fstab"
			return 1
		fi

		cat $eq_fstab >>$new_fstab				# tack on the one equerry made for us
		rm -f $eq_fstab
		$NG_ROOT/bin/ng_dosu -v fstab				# install our new fstab
		return $?
	else
		log_msg "would have installed a new fstab from $eq_fstab"
	fi

	return 0
}


# ------------- various flavours report mounts differently ----------------
#bsd	(assumed read/write unless marked read/only)
#	/dev/ad0s1e on /tmp (ufs, local, soft-updates)
#	/dev/acd0 on /cdrom (cd9660, local, read-only)
#
#linux	(specific markings)
#	/dev/sdb8 on /ningaui/fs03 type ext3 (rw,errors=panic)
#
#sun 	(odd format)
#	/tmp on swap read/write/setuid/xattr/dev=2 on Thu Jan 18 11:03:03 2007


# for now we support  linux, bsd, and sun style mount output
# for bsd, if it does not say 'read-only' then we assume it is r/w
# the expected mountpoint must be given.  there is no easy way to verify
# that the partition with label foo is actually mounted as /ningaui/foo
# so we assume that the mount was done using the label option and that the 
# system got it right.  
# output is one of three tokens: mounted-rw, mounted-ro, or unmounted.
#
# list all mounted filesystems to stdout as name rw|ro
function dump_mount
{
        if [[ $(ng_uname) == "SunOS" ]]                 # grumble
        then
                #/ningaui/fs00 on /dev/dsk/c1t1d0s6 read/write/setuid/intr/largefiles/logging/xattr/onerror=panic/dev=1d80006 on Tu
                mount | awk -v what="${1}" '
                        {
				mpt = $1;

                                mounted = 1;
                                read_write = 0;
                                n = split( $4, a, "/" );
                                for( i = 1; i <= n; i++ )
                                {
					if( a[i] == "write" )
					{
						read_write = 1;
						break;
					}
                                }
                                printf( "%s %s\n", mpt, read_write ? "rw" : "ro" );
                        }
                '
        else
                mount | awk '
                {
			mpt = $3;
			split( $0, a, "(" );
			split( a[2], b, ")" );
			opts = b[1];
			gsub( " ", "", opts );
			x = split( opts, a, "," );
                                
			read_write = 1;
			for( i = 1; i <= x; i++ )
			{
				if( a[i] == "rw" )
					read_write = 1;
				if( a[i] == "ro" )
					read_write = 0;
				if( a[i] == "read-only" )        
					read_write = 0;
			}

                        printf( "%s %s\n", mpt, read_write ? "rw" : "ro" );
                }

                '
        fi
      
        return 0
}   

# same mount representation discussion applies here too
# check the state of a specific filesystem
function ck_state
{
	if [[ $(ng_uname) == "SunOS" ]]			# grumble
	then
		#/ningaui/fs00 on /dev/dsk/c1t1d0s6 read/write/setuid/intr/largefiles/logging/xattr/onerror=panic/dev=1d80006 on Tu
		mount | awk -v what="${1}" '
			{
				if( $1 == what )
				{
					mounted = 1;
					read_write = 0;
					n = split( $4, a, "/" );
					for( i = 1; i <= n; i++ )
						{
						if( a[i] == "write" )
						{
							read_write = 1;
							break;
						}
					}
				}	
			}

			END {
				printf( "%s\n", mounted ? (read_write ? "mounted-rw" : "mounted-ro") : "unmounted" );
			}
		'
	else

		if [[ ! -d $1 ]]			# short circuit if directory is not there -- cannot be mounted
		then
			echo unmounted;
			return
		fi

		mount | awk -v what="${1}" '
		BEGIN {
			mounted = 0;
		}

		{
			if( $3 == what )
			{
				mounted = 1;

				split( $0, a, "(" );
				split( a[2], b, ")" );
				opts = b[1];
				gsub( " ", "", opts );
				x = split( opts, a, "," );

				read_write = 1;
				for( i = 1; i <= x; i++ )	
				{
					if( a[i] == "rw" )
						read_write = 1;
					if( a[i] == "ro" )
						read_write = 0;
					if( a[i] == "read-only" )
						read_write = 0;
				}
			}
		}

		END {
			printf( "%s\n", mounted ? (read_write ? "mounted-rw" : "mounted-ro") : "unmounted" );
		}
		'
	fi

	return 0
}

# attempt to mount the device passed in. If we find the magic cookie as the first token on a line in 
# fstab, then we are allowed to update the fstab and try the mount.  If we dont find the cookie, or the
# allow mounts pinkpages variabel is 0, then we don't mount and exit bad. 
# expect parms: device/label mtpt fstab-entries-filename
# further, the mountpoint directory must exist with the file MOUNT_POINT inside as a regular file. If 
# the mount point directory is not inside of $NG_ROOT, then a symlink $NG_ROOT/xxxx where xxxx is the last
# portion of the mount point name (e.g. site in /m/ningaui/site) is expected and created if not there. 
# 
function mount_it
{
	typeset new_fstab=$TMP/PID$$.fstab 
	typeset eq_fstab=$3
	typeset lname=""

	if ! gre "^$fstab_cookie" $sys_fstab >/dev/null 2>&1
	then
		log_msg "mount/unmount not permitted: cookie missing from fstab"
		cleanup 1
	fi

	if (( $allow_mounts > 0 ))
	then
		# we do not use install_fstab because we create it here, use it next, and then if all ok (mtpts created) install it 
		sed "/$fstab_cookie/q" <$sys_fstab >$new_fstab		# capture stuff off limits for us to change, including cookie line
		cat $eq_fstab >>$new_fstab				# tack on the one equerry made for us
		rm -f $eq_fstab
	
		gre "$1" $new_fstab|tail -1 | read dev mtpt junk
		if [[ ! -d $mtpt ]]					# /n/rmfs0223 or /ningaui/rmfs0223
		then
			if ! mkdir $mtpt >/dev/null 2>&1		# we expect this to fail when using /l/rmfsxxxx style mounting so trash messages
			then
				$NG_ROOT/bin/ng_dosu -v mkmdir $mtpt		# if its an pk/rmfs fileystem, then this should work
				if (( $? != 0 ))
				then
					log_msg "unable to create mtpt: $mtpt [FAIL]"
					return 1
				fi
				log_msg "created mtpt: $mtpt [OK]"
				$NG_ROOT/bin/ng_dosu -v mkmtpt $mtpt		# add MOUNT_POINT to directory
			else
				log_msg "created mtpt: $mtpt [OK]"
				chown $prod_id:$prod_id $mtpt
				chmod 775 $mtpt
				touch $mtpt/MOUNT_POINT 
			fi

		else
			if [[ -f $mtpt/MOUNT_POINT ]]			# must have this file 
			then
				log_msg "found mout point indicator [OK]"
			else
				log_msg "missing MOUNT_POINT flag in directory: $mptp [FAIL]"
				return 1
			fi
		fi

		if [[ $mtpt != $NG_ROOT/* ]]				# mtpt is outside of ningaui root, we must ensure a sym link to it
		then	
			lname=${mtpt##*/}
			lname=${lname#ng_}				# we point fs00 at ng_fs00 and site at ng_site 
			if [[ ! -L $NG_ROOT/${lname} ]]			# does not exist; make one
			then
				ln -s $mtpt $NG_ROOT/${lname}
				log_msg "created symlink $NG_ROOT/${lname} -> $mtpt"
			else
				log_msg "found symlink in NG_ROOT [OK]"
			fi
		fi

		$NG_ROOT/bin/ng_dosu -v fstab				# (finally) install our new fstab
		if (( $? == 0 ))
		then
			$NG_ROOT/bin/ng_dosu -v mount ${1:-foo} ${2:-bar}
			rc=$?
			if (( $rc == 0 ))
			then
				$NG_ROOT/bin/ng_dosu -v chown ${NG_PROD_ID:-ningaui}:${NG_PROD_ID:-ningaui} ${2:-bar}
			fi
		else
			log_msg "could not update fstab"
			rc=1
		fi
	else
		log_msg "mount/unmount not permitted: NG_EQ_ALLOW_MOUNTS set to 0"
		log_msg "would have issued: ng_dosu mount $1"
		rc=1
	fi

 	return $rc	
}

# non-distructive version. does not need to have entries in fstab; does require special support from ng_dosu
# to do the commands.  currently dosu does not support this
#
# We assume $1 is the label, $2 the mountpoint, $3 rw/ro option $4 is the type. The label may be of the 
# form /dev/<dname> or [a-z]*.  
# on a linux box where mount supports -L we will assume that `mount -L name -o option mountpoint` will work
# on a freebsd box where mount does not support -L, but allows us to build a small fstab and use it with 
#	the -F option we will do `mount -F tmpfile mountpoint 
function nd_mount_it
{
	case $(uname -s) in 
		FreeBSD|Darwin)
			typeset fstab=$TMP/PID$$.fstab
			if [[ $1 == "/"* ]]
			then
				echo "$1 $2 $4 $3 0 0" >$fstab
			else
				echo "LABEL=$1 $2 $4 $3 0 0" >$fstab
			fi
			if (( verbose > 0 ))
			then
				cat $fstab >&2
			fi

			cmd="$NG_ROOT/bin/ng_dosu -v fmount $fstab"
			;;

		Linux)	
			if [[ $1 == "/"* ]]
			then
				cmd="$NG_ROOT/bin/ng_dosu mount $3 $4 $1 $2"
			else
				cmd="$NG_ROOT/bin/ng_dosu lmount $1 $3 $4 $2"
			fi
			;;

		*)	log_msg "system type unsupported for mounting"
			return 1
			;;
	esac

	if (( $allow_mounts > 0 ))
	then
		verbose "executing: $cmd"
		eval $cmd
		return $?
	else
		log_msg "mounts disabled, would have executed: $cmd"
		return 1				# if we dont do the request, equrerry must know so fail even if it was manual -n
	fi

	return 2		# shouldnt, but will standout if it does
}

# invoke dosu to do the unmount. then we must clean up the symlink so that we dont report it as an 
# unmounted filesystem.  If the mtpt is directly in NG_ROOT, then we remove the mountpoint directory
# so that we dont report it as unmounted. 
function umount_it
{
	typeset mtpt=$1

	if (( $allow_mounts > 0 || $force > 0  ))
	then
		$NG_ROOT/bin/ng_dosu -v umount $mtpt
		if (( $? != 0 ))
		then	
			log_msg "cannot unmount $mtpt [FAIL]"
			cleanup 1
		fi

		if [[ $mtpt != $NG_ROOT/* ]]				# mtpt is outside of ningaui root, we must trash the link to it
		then	
			if [[ -L $NG_ROOT/${mtpt##*/} ]]		# exists, ditch it
			then
				rm $NG_ROOT/${mtpt##*/}
				log_msg "deleted symlink $NG_ROOT/${mtpt##*/} -> $mtpt [OK]"
			else
				log_msg "symlink not found in NG_ROOT [OK]"	# odd but ok
			fi
		else
			if [[ -f $mtpt/MOUNT_POINT ]]
			then
				rm $mtpt/MOUNT_POINT				# if this fails we catch it in rmdir
				if ! rmdir $mtpt				# MUST use rmdir to prevent accidents
				then
					log_msg "after unmount, mtpt directory was not empty: $mtpt [FAIL]"
					ng_log $PRI_CRIT "after unmount, mtpt directory is not empty: $mtpt"
				else
					log_msg "mtpt directory removed: $mtpt [OK]"
				fi
			else
				log_msg "directory did not contain MOUNT_POINT indicator; not removed: $mtpt"
			fi
		fi
	else
		log_msg "mounts disabled, would have executed: ng_dosu -v umount $1"
		cleanup 1				# if we dont do the request, equrerry must know so fail even if it was manual -n
	fi
}

# find the next available rmfsXXXX filesystem. Gets the first fileystem whose parm
# var has UNALLOC as first token. If no unallocated ones exist, then we add one to 
# the last disk noticed. The one flaw we have here is that if someone manually deletes
# a nar var, we will not fill in the gap. 
function find_next
{
	typeset v=""
	typeset dname=""

	ng_nar_get -P "^$eqid/$fsid.*:parms" | sort | awk -v fsid=$fsid '
		BEGIN {
			flen = length( fsid );		# size of the constant fsid tag on filesytem name
			last_fs = -1;
			found = "";
		}

		function vname2dig( vname, 	dig, x )
		{
			x = index( vname, "/" );
			dig = substr( vname, x+flen+1 );		# fs number of this puppy
			if( (x = index( dig, ":" )) > 0 )
				dig = substr( dig, 1, x-1 );
			return dig;
		}

		{
			x = index( $0, "=" );
			vname = substr( $0, 1, x-1 );			# expect equerry/fsname:parm
			if( substr( $o, x+2, 7 ) == "UNALLOC" )		# use first unallocated one we find
			{
				found = sprintf( "%s%s", fsid, vname2dig( vname ) );
				exit( 0 );
			}

			
			this_fs = vname2dig( vname );
			nlength = length( this_fs );
			if( this_fs+0 != last_fs +1 )			# gap
			{
				fmt = sprintf( "0%dd", last_fs == -1 ? 4 : nlength );
				found = sprintf( "%s%" fmt, fsid, last_fs + 1 );		# craft new name
				exit( 0 );
			}

			last_fs = this_fs;
		}

		END {
			if( found )
				printf( "%s\n", found );
			else
			{
				fmt = sprintf( "0%dd\n", last_fs == -1 ? 4 : length( last_fs ) );
				printf( "%s%" fmt, fsid, last_fs + 1 );		# craft new name
			}
		}
	'
}


# set a variable in narbalek; also supports the reserve and drop commands
function set_var
{
	typeset what=$1         # what to do
	typeset disk=$2         # disk it is for
	shift 2

	typeset val=""

	case $what in               
        	state)                  
			if (( $force > 0 ))
			then
				host=$1
				shift
                		nvname="$eqid/$disk:$host.state"
			else
				log_msg "only ng_equerry should set the state; use -f if you feel you must"
				cleanup 1
			fi
                	;;
           	 
		drop)
			val="UNALLOC "
                	nvname="$eqid/$disk:parms"
			ng_nar_get $nvname | read old
			verbose "drop: old parms were: ${old/ALLOC /}"
			ng_log $PRI_INFO $argv0 "drop: old parms: $nvname ${old/ALLOC /}"
	
			val2=""
                	nvname2="$eqid/$disk:host"
			ng_nar_get $nvname2 | read old
			verbose "drop: old host assignment(s) were: ${old}"
			ng_log $PRI_INFO $argv0 "drop: old host: $nvname2 ${old}"
			;;

        	reserve)
			val="RESERVED reserved by $USER on $(ng_dt -p)"
			if [[ $disk == "auto" ]]
			then
				find_next | read disk
			fi
                	nvname="$eqid/$disk:parms"

			if (( forreal < 1 ))
			then
				log_msg "next filesystem name would be: $disk"
				cleanup 0
			fi

			log_msg "reserving $disk; use '$argv0 set parm $disk <parameters>' to activate"
			ng_log $PRI_INFO $argv0 "reserve: $disk"
                	;;

        	rparm|rparms)		# reserve; user has supplied all needed info (mostly for gen command use)
			val="RESERVED "
                	nvname="$eqid/$disk:parms"
                	;;
	
        	parm|parms)
			val="ALLOC "
                	nvname="$eqid/$disk:parms"
                	;;
	
        	host)
                	nvname="$eqid/$disk:host"
                	;;
	
        	*)      log_msg "unrecognised variable type: $what"
                	cleanup 1
                	;;
	esac                

	while [[ -n $1 ]]		# ng_nar_req needs all of the value as a quoted token
	do
        	val="$val$1 "
        	shift        
	done
	val="${val% }"

	if [[ $nvname == *"="* ]]
	then
		log_msg "abort: illegal narbalek variable; cause is likely 'fstab=value' on the command line. dont use '='"
		cleanup 1
	fi

	if (( $forreal > 0 ))
	then
		verbose "setting $nvname=$val"
		ng_nar_set $nvname="$val"

		if [[ -n $nvname2 ]]				# some cases cause us to set two vars
		then
			verbose "setting $nvname2=$val2"
			ng_nar_set $nvname2="$val2"
		fi

		send_update			# send word to equerry that the parms were shifted
	else
		log_msg "not set (-n mode)  $nvname=$val"
	fi
}

function show
{
	ng_nar_get -P "^$eqid/.*${1:-parm}" | awk -F "#" '
		/UNALLOC/ {next;}
		/RESERVED/ {next;}
		{
			x = index( $1, "=" );
			
			gsub( "\"", "", $1 );
			gsub( "\t", " ", $1 );
			gsub( " +", " ", $1 );
			gsub( "ALLOC", "", $1 );
			a[1] = substr( $1, 1, x-1 );
			a[2] = substr( $1, x+1 );
			split( a[1], b, "/" );

			printf( "%20s %s\n", b[2], a[2] == " " ? " <empty>" : a[2] );
		}
	' |sort
}

# fetch and list all nar vars that indicate reserved
function fetch_reserved
{
	typeset dname=""
	typeset v=""
	ng_nar_get -P "^$eqid/$fsid.*:parms" | gre reserved |sort | while read v
	do
		dname=${v%%=\"*}
               	dname=${dname##*/}
               	dname=${dname%%:*}	# get the disk name
           	 
               	v=${v##*=\"}
               	v=${v%%\"*}    		# pluck the value

		echo "$dname ${v/RESERVED/}"
	done
}

# fetch a var from narablek; makes state vars look as though they were one
function fetch_var
{
	typeset what=$1         # what to do
	typeset disk=$2         # disk it is for
	shift 2

	case $what in               
        	state)                  
			if (( $all < 1 ))
			then
                		nvname="$eqid/$disk:$1.state"
			else
                		nvname="^$eqid/$disk:.*[.]state"
			fi
                	;;
           	 
        	parm|parms)
			all=0
                	nvname="$eqid/$disk:parms"
                	;;
	
        	host)
			all=0
			val=""
                	nvname="$eqid/$disk:host"
                	;;

        	*)      log_msg "unrecognised variable type: $what"
                	cleanup 1
                	;;
	esac                

	if (( $all > 0 ))
	then
        	ng_nar_get -P $nvname | gre -v RESERVED | while read v            # expect equerry/diskname:host.varname
        	do
                	hname=${v#*:}   
                	hname=${hname%%.*}
            	 
                	v=${v##*=\"}
                	v=${v%%\"*}    
                	if (( long_list > 0 ))
                	then     
                        	echo "$hname=$v"              
                	else     
                        	val="$val$hname=$v "              
                	fi
        	done
	
        	if (( $long_list == 0 ))
        	then
                	echo ${val/ALLOC }
        	fi
	
	else
        	ng_nar_get $nvname | read val
                echo ${val/ALLOC/}
	fi
}

# send an update message to equerry to force a read of nar vars now
function send_update
{
	echo "updated" | ng_sendtcp -e . -t $timeout $host:$port 2>/dev/null 
}

# check the status of all NG_ROOT/xxxx filesystems and the state of fileystems
# that are managed by equerry.  Exits bad if any that should be mounted are not.
# intended for use by things like ng_node to ensure that all is well before 
# turning on green-light.
function verify_mounts
{
	typeset tfile=$TMP/PID$$.tfile

	rc=0

	if [[ $sys_fstab == /etc/vfstab ]]		# bloody sun nodes
	then
		col=3;
	else
		col=2;
	fi

	verbose "waiting for filesystem list from equerry"
	touch $tfile
	while [[ ! -s $tfile ]]		# get a list -- must wait for something in the file to ensure equerry is up and stable
	do
		echo "list" | ng_sendtcp -e . -t 90 $host:$port   >$tfile 2>/dev/null
		sleep 2
	done

	awk -v verbose=$verbose '
		BEGIN { state = 0; }
		/target=/ {
			split( $3, a, "=" );
			split( $4, b, "=" );
			if( a[2] != b[2] )
			{
				state = 1;
				printf( "equerry managed filesystem not stable: %s targ=%s state=%s   [FAIL]\n", $2, a[2], b[2] ) >"/dev/fd/2";
			}
			else
				if( verbose )
					printf( "equerry managed filesystem stable: %s targ=%s state=%s   [OK]\n", $2, a[2], b[2] ) >"/dev/fd/2";
		}
		END {
			exit( state );
		}
	' <$tfile
	rc=$?

	# test directories immediately under $NG_ROOT, and the directories pointed to by symlinks to ensure that 
	# they do not have a MOUNT_POINT file. 

	cd $NG_ROOT
	open=0
	ls | while read d
	do
		if [[ -d $d || -L $d  ]]
		then
			if [[ -f $d/MOUNT_POINT ]]
			then
				log_msg "filesystem not mounted on: $NG_ROOT/$d [FAIL]"
				open=1
			else
				verbose "is mounted: $d [OK]"
			fi
		fi
	done
	rc=$(( $rc + open ))


	# old code -- we used to just test anything in fstab that was mounted uner NG_ROOT, but policy has changed
	# this code can be deleted as soon as all nodes are playing with the new rules; it does not hurt to test
	# for these on newly sequenced nodes as there should not be any NG_ROOT enteries in fstab
	#
	awk -v col=$col -v verbose=$verbose -v ng_root=$NG_ROOT '

		/^#equerry-begin/ { exit( 0 ); }		# we dont test equerry managed things here

		index( $(col), ng_root) > 0  {
			if( substr( $1, 1, 1 ) == "#" || index( $0, "noauto" ) )
			{
				if( verbose )
					printf(  "commentted out or noauto: %s [OK]\n", $(col) )>"/dev/fd/2";
				next
			}
			
			print $(col);				# generate a list of mtpts to check
		}
	' <$sys_fstab |while read mpt
	do
		ck_state $mpt | read state
		if [[ $state == "unmounted" ]]
		then
			rc=$(( $rc + 1 ))
			log_msg "filesystem not mounted: $mpt [FAIL]"
		else
			verbose "filesystem mounted; $mpt [OK]"
		fi	
	done

	return $rc
}

# detach filesystem ($1) from current node and prepare for move to another node
# this is intended to be executed only on the node that has the :rw privledge
# as established in equerry/device:host narbalek variable.
# To detach we:
#	set narbalek parameters to have equerry unmount the fs
#	wait for unmount
#	suss a list of files from dbd that exist on the fs
#	if target node defined, send list to target
#
# after this executes, the physical disk may be moved and the attach command
# can be executed on the target node.
#
function detach
{
	typeset device=$1
	typeset target=$2
	typeset thisnode="$(ng_sysname)"
	typeset troot=""
	

	if [[ -n $target ]]
	then
		ng_rcmd $target ng_wrapper -r|read troot
		if [[ -z $troot ]]
		then
			log_msg "cannot detach: cannot contact target: $target [FAIL]"
			return 1
		fi
	fi

	typeset dstate="$(ng_nar_get equerry/$device:host)"	# desired state

	if [[ -z $dstate ]]
	then
		log_msg "cannot find host assignment for device ($device) in narbalek [FAIL]"
		return 1
	fi

	if [[ $dstate != *$thisnode:* ]]
	then
		log_msg "current host assigment does not list $thisnode  for device ($device): $dstate [FAIL]"
		return 1
	fi

	if [[ $dstate != *$thisnode:rw* ]]
	then
		log_msg "current host assigment does not list $thisnode:rw  for device ($device): $dstate [FAIL]"
		return 1
	fi

	nstate=""
	for ds in $dstate	# create a new host asssignment list with this host as unmounted
	do
		if [[ $ds == *:rw ]]	# need to make sure that rw assignment is for this node
		then
			if [[ $ds != $thisnode:rw ]]
			then
				log_msg "cannot detach filesystem; this node ($thisnode) is not rw target ($ds) [FAIL]"
				return 1
			fi
		else
			if [[ $ds == $thisnode:ro || $ds == $thisnode:ignore ]]
			then
				log_msg "cannot detach filsystem; this node does not have rw target ($ds) [FAIL]"
				cleanup 1
			fi
		fi

		if [[ $ds == $thisnode:* ]]	# drop this host from list; changes state to unmount by default and keeps nar var cleaner
		then
			:
		#	nstate="$nstate$thisnode:unmount "
		else						# keep other hosts the same
			nstate="$nstate$ds "
		fi
	done

	# if we exit the loop, we assume that ourhost:rw was set and that nstate has the new desired host state flags
	verbose "setting equerry/$device:host to $nstate"
	ng_nar_set equerry/$device:host="${nstate% }"

	verbose "waiting for equery to unmount $device"
	typeset msg_count=0
	while true
	do
		if (( ${verbose:-0} > 0 )) 
		then
			if (( msg_count <= 0 ))
			then
				verbose "$(ng_eqreq list|gre "$device")"
				msg_count=30
			else
				msg_count=$(( $msg_count - 1 ))
			fi
		fi

		if ng_eqreq list|gre "$device.*target=unmounted.*current=unmounted" >/dev/null 
		then
			verbose "equerry reports $device unmounted"
			break;
		fi

		sleep 2
	done
	

	flist="$NG_ROOT/site/rm/dbd_list.$device"
	ng_rm_db -p |grep "$NG_ROOT/$device/" >$flist		# suss out a copy
	ng_rm_db -d <$flist					# remove them
	
	if [[ -n $target ]]
	then
		if ! ng_ccp -d $troot/site/rm/dbd_list.$device $vflag $target $flist	# send the list
		then
			log_msg "unable to send file list ($frlist) to target ($target); filesystem globally unmounted [FAIL]"
			return 1
		fi

		mv -f $flist $flist.sent		# keep as backup, but will not load here by default
		verbose "filesystem $device unmounted, files removed from rm_db and sent to $target:$troot/site/rm/${flist##*/}"
	else
		verbose "filesystem $device unmounted, files removed from rm_db and left in $flist"
	fi

	ng_flist_send

	return 0
}

# set the filesystem to be mounted on the current node and load the file instance information 
# into rm_dbd. To do this we:
#	ensure that no other node has rw privledges (according to nar vars)
#	create a new equerry host var for the device with this host having rw 
#	set the var and wait for equerry to mount it
#	load the instance information into rm_db (if fname provided, or default exists)
#
function attach
{
	typeset device=${1##*/}			# device to attach to this node
	typeset instf=$2			# file with instance info (optional)
	typeset thisnode="$(ng_sysname)"

	typeset dstate="$(ng_nar_get equerry/$device:host)"	# desired state

	if [[ $dstate == *ERROR* ]]		# it can be empty, but not this
	then
		log_msg "unable to get host assignment for $device from narbalek [FAIL]"
		return 1
	fi

	if [[ $dstate != *$thisnode:* ]]		# it is ok if we are not listed, but need to be there for loop
	then
		dstate="$thisnode:unmount$dstate "
	fi

	typeset nstate=""
	for ds in $dstate					# there can be multiple
	do
		if [[ $ds == *:rw ]]			# there must not already be a :rw assignment for the filesystem
		then
			log_msg "cannot attach filesystem; a node already has rw assignment: $ds [FAIL]"
			return 1
		else
			if [[ $ds == $thisnode:* ]]
			then 
				nstate="$nstate$thisnode:rw "		# change our entry
			else
				nstate="$nstate$ds "			# allo thers stay the same
			fi
		fi
	done

	verbose "setting equerry/$device:host to $nstate"
	ng_nar_set equerry/$device:host="${nstate% }"

	verbose "waiting for equery to mount $device"
	typeset msg_count=0
	while true
	do
		if (( ${verbose:-0} > 0 )) 
		then
			if (( msg_count <= 0 ))
			then
				verbose "$(ng_eqreq list|gre "$device")"
				msg_count=30
			else
				msg_count=$(( $msg_count - 1 ))
			fi
		fi

		if ng_eqreq list|gre "$device.*target=mounted.*current=mounted" >/dev/null 
		then
			verbose "equerry reports $device mounted"
			break;
		fi

		sleep 2
	done

	
	if [[ -z $instf ]]
	then
		instf="$NG_ROOT/site/rm/dbd_list.$device"		# default list
	fi

	typeset rc=0
	if [[ -f ${instf:-foo} ]]
	then
		verbose "loading instance data into rm_db from $instf"
		ng_rm_db -a <$instf
		rc=$?

		if (( $rc == 0 ))
		then
			verbose "instance data load successful; removing $instf"
			rm -f $instf						# prevent accidental reload

			ng_flist_send
		fi
	else
		log_msg "cannot find instance data ($instf); nothing loded into rm_db [WARN]" 
	fi

	return $rc
}


# -------------------------------------------------------------------
ustring="[-a] [-e] [-F fsid] [-i eqid] [-n] [-p port] [-s host] [-t timeout] command"
port=${NG_EQUERRY_PORT:-4444}
allow_mounts=${NG_EQ_ALLOW_MOUNTS:-1}		# by default mounts are allowed
forreal=1
stop=0
timeout=10
host=localhost
long_list=0
all=0
force=0				# set by -f to allow state var changes
eqid=equerry			# id used to name narbalek vars
fsid=rmfs			# by default all filesystems are expected to be rmfs%04d
fstab_cookie="#equerry-begin"	# special string that must be first characters on a record in fstab; we are allowed to delete/add anything below
prod_id=${NG_PROD_ID:-ningaui}
sys_fstab=/etc/fstab		# the sane place, but of course sun must be different



if [[ ! -e $sys_fstab ]]
then
	if [[ -e /etc/vfstab ]]
	then
		sys_fstab=/etc/vfstab
	fi
fi

while [[ $1 == -* ]]
do
	case $1 in 
		-a)	all=1;;
		-e)	stop=1;;
		-f)	force=1;;
		-F)	fsid=$2; shift;;
		-i)	eqid=$2; shift;;
		-l)	long_list=1;;
		-n)	forreal=0; allow_mounts=0;;
		-p)	port=$2; shift;;
		-s)	host=$2; shift;;
		-t)	timeout=$2; shift;;

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

if (( $stop > 0 ))
then
	set xit4360
fi

		# the capitalised command names are intended to be invoked only by equerry 
		# and thus are not documented on the man page. Any command not directly
		# handled by this script is passed into equerry.
case $1 in 
	INSTALL_FSTAB)		# $2 assumed to be the name of the fstab segment that equerry created
		shift
		install_fstab "$@"
		cleanup $?
		;;

	MOUNT)			# invoked by equerry to mount a fs; we execute rm_arena to recalculate the arena after any mount/umount
		shift
		mount_it "$@"		# expect device/label mtpt fstab-entries-filename
		rc=$?			# our exit status is the status of the mount, not anything else!

		log_msg "updating repmgr arena..."
		ng_rm_arena	init		# run regardless of status; may need to delete fs that could not be mounted
		ng_rm_arena	pkinit		# initialise parkinglots too

		# if the system is up, not as we are booting -- node start takes care of this 
		# we also don't do this if the mount failed
		if ng_wrapper -s && (( $rc == 0 ))
		then
			ng_spaceman	-v # if this was a new filesystem, we need to populate it

			c=0
			ng_wreq dump|gre resource|gre RM_SYNC | while read j1 rname junk
			do
				c=$(( $c + $(ng_wstate $rname) ))
			done
			if (( $c <= 0 ))
			then
				log_msg "no rmsync jobs scheduled; starting rmsync"
				ng_rm_sync &
			else
				log_msg "$c rmsync related jobs queued in woomera; not starting rmsync"
			fi
	
	
			log_msg "starting any needed valets..."
			ng_start_valet
		fi
		cleanup $rc
		;;

	UMOUNT) 		# invoked by equerry to unmount a fs
		shift
		umount_it "$1"
		rc=$?
		log_msg "updating repmgr arena..."
		ng_rm_arena	init
		cleanup $rc
		;;

	# =====================================================================================================================

	attach)
		shift
		attach ${1:-device-unspecified} $2
		cleanup $?
		;;

	ck_state)		# check to see if $2 semes to be mounted on this system
		verbose "eqreq: checking state: $2"
		ck_state $2
		cleanup $?
		;;

	detach)
		shift
		detach ${1:-device-unspecified} ${2}
		cleanup $?
		;;

	dump_mount)
		dump_mount;
		cleanup $?
		;;

	drop)	
		shift
		set_var drop $1
		cleanup $?
		;;

	gen)	
		shift
		gen_nar_cmds  <${1:-/dev/fd/0}
		cleanup $?
		;;

	get)
		shift
		if [[ $1 == "reserve"* ]]
		then
			fetch_reserved
		else
			fetch_var "$@"
		fi
		cleanup $?
		;;

	poke)	send_update;			# cause equerry to refresh things
		;;

	reserve)
		set_var reserve ${2:-auto}	# auto causes us to generate next one if they left off the name
		cleanup $1
		;;

	set)			# user invoked to set a variable
		shift
		set_var "$@"
		cleanup $?
		;;

	show)	
		show $2
		cleanup $?
		;;

	test)
		shift
		$*
		cleanup $?
		;;

	verify) 
		case $2 in 
			mount*)
				verify_mounts
				cleanup $?
				;;

			*)	log_msg "unrecognised verify subcommand: ${2:-not-supplied}"
				cleanup 1
				;;
		esac
		;;
		

	*)			# not recognised; assume we send it to the daemon
		echo "$@" | ng_sendtcp -e . -t $timeout $host:$port
		;;
esac


cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_eqreq:Request infterface for equerry)

.** -----------------------------------------------------------------------------------------------------------
.** NOTE:
.** The commands MOUNT, UMOUNT, INSTALL_FSTAB are NOT documented in the man page as they are intended for use 
.** only by equerry and not as an interface to equerry.
.** -----------------------------------------------------------------------------------------------------------

&space
&synop	
&break	ng_eqreq [-p port] [-s host]  -e
&break  ng_eqreq [-v] attach filesystem [rm_db-data]
&break	ng_eqreq [-p port] [-s host] ck_state mount-point-name
&break  ng_eqreq [-v] detach filesystem [target-node]
&break  ng_eqreq [-F fsid] drop filesytem-name 
&break  ng_eqreq [-a] [-F fsid] [-l] get {state | parm | host} filesytem-name [hostname]
&break  ng_eqreq gen [<]gen-data-file
&break  ng_eqreq [-F fsid] get reserved
&break	ng_eqreq [-p port] [-s host] list [fs-name]
&break	ng_eqreq [-F fsid] reserve [filesystem-name]
&break  ng_eqreq [-F fsid] set parm filesytem-name value
&break  ng_eqreq [-F fsid] set host filesytem-name value
&break  ng_eqreq [-F fsid] set state filesytem-name host value
&break	ng_eqreq [-p port] [-s host] verbose n
&break	ng_eqreq [-v] verify mounts 


&space
&desc	&This is inteface to the &lit(ng_equerry) daemon, and a helper tool 
	for the daemon itself.  It can be used to set narbalek variables that 
	are necessary for equerry operation, list the current state of filesystems
	as perceived by equerry, adjust the verbose level used by equerry, and others.

&subcat Setting Narbalek Variables
	Three types of narbalek variables are used by equerry, two of them are 
	user maintained variables and thus can be set by &this. The &lit(host) variable
	lists easch host that is expected to mount the filesystem.  The value of a host 
	variable are space separated pairs of &lit(hostname^:state) where state is 
	rw (read/write) or ro (read/only).  If a host is not to mount a filesystem, then
	it should not be listed.  
&space
	The filesystem's parameters that equerry needs to be aware of are listed on 
	the &lit(parm) narbalek variable for the filesystem.  The values that can be 
	supplied to equerry via this variable are listed on the &lit(ng_equerry) manual page. 
	They do include the filesystem label, mount point, and options. 
&space
	The narbalek variable that indicates the state of a filesystem should be managed only
	by ng_equerry. The state value may be changed using &this (if force mode is set as 
	a command line option).  This allows the state to be changed in the event that a 
	node crashes and does not come back in order to allow equerry to alter the state itself. 
	The value of a state variable is one of three strings: mounted-rw, mounted-ro, or unmounted.
&space
&subcat	Tabular Generation
	A table of filesystem and host information can be used to generate the various equerry
	commands that setup the narbalek parameters. &This can be invoked with the &lit(gen)
	command and the name of the table file supplied on the command line as the only other 
	positional parameter.  The table is expected to be newline terminated records with the 
	following fields (in the order specified)
&space
&beglist
&item	The device name or label. If just the label is given (e.g. rmfs0234), then the 
	default device directory path (!default_device_path) is used.  If the default 
	device path is not given, then the path path is assumed to be &lit(/dev/)&ital(type). 
	The string "label_only" can be used as the default device path which causes the 
	label to be added without a device path.  The assumption is that LABEL=xxx will be 
	used in /etc/fstab and no device path is needed. 
&space
&item	The mount point directory. If not given, a dot used, then the mount point is assumed to be
	&lit(NG_ROOT.) This is the &stress(the directory name) of the directory containing the mountpoint
	and NOT the path of the mountpoint directory.  Thus, &lit(/l) should be  given for &lit(/dev/ufs/rmfs0300)
	if  the mountpoint is to be &lit(/l/rmfs0300.)
&space
&item	The filesystem type (e.g. ufs).
&space
&item	Any options needed.  The options rw, ro, and noauto should NOT be supplied.
&space
&item	A token describing the physical location of the disk holding the filesystem.  This
	is used only as a human reference and is ignored by all equerry processes. 
&space
&item	The host name state pair, separated by a colon, indicating which hosts mount the filesystem. 
	The word drop may be used to indicate that the once allocated filesystem has  been 
	dropped from equerry management.
&endlist

&space
&litblkb
 # . in mtpt dir col will be assigned the default value in next line
 !default_mtpt_dir /l
 
 #if label does not have a dvice directory, the device default is used
 # label_only indicates that fstab has LABEL= and no device dir is needed

 !default_device_dir label_only
 #fsys-id         label/dev       mount          type    option  src     flags           host:state

 rmfs0210site     .              /l/ng_site      ufs     .       pk21    PRIVATE,NOACTION roo06:rw
 rmfs0210apps     .              /l/ng_apps      ufs     .       pk21    .                roo06:rw
 rmfs0210tmp      .              /l/ng_tmp       ufs     .       pk21    PRIVATE,NOACTION roo06:rw 
 rmfs0210fs00     .              /l/ng_fs00      ufs     .       pk21    PRIVATE          roo06:rw 
 rmfs0234         .              /l/rmfs0234     ufs     .       pk23    .                roo06:rw
 rmfs0235         .              /l/rmfs0235     ufs     .       pk23    .                roo06:rw
 pkfs0236         .              /l/pkfs0236     ufs     .       pk23    .                roo06:rw

dib99_ng_tmp    /dev/da0s1f     /ningaui/tmp    ufs     .       local   PRIVATE,NOACTION        dib99:rw

&litblke
&space
	The result of executing the &lit(gen)  command is a list of &lit(ng_eqreq) commands that can be run 
	to set the necessary narbalek variables.  

&space
&subcat Reserving A Filesystem
	Once the parameters of a filesystem have been defined, equerry will begin managing the 
	filesystem.  The reserve command allows a filesystem name place holder to exist without
	having equerry begin management of the filesystem.  
	The primary purpose of the reserve command is to block the filesystem name from being
	automatically generated for another user. 
&space
	Two forms of the reserve command are recognised: specific and automatic. In specific
	mode, the last token on the command line is the name of the filesystem to reserve. If 
	the name is omitted, then &this will generate the next filesystem name with the 
	syntax &lit(rmfsXXXX;)  this includes the reuse of filesystem names that have been dropped.
&space
	The reservation of a filesystem name before defining its parameters is not a requirement, but 
	meerely a convenience for the cluster administrator.  Reserved filesystem names can be listed
	with the &lit(get reserved) command. 
&space
&subcat	Dropping A Filesystem
	If a filesystem is to be removed from the control of equerry, then it must be dropped.
	The act of dropping a filesystem communicates to equerry that it no longer must track
	the state of the filesystem, and makes the filesystem name available for reuse. 

&space
&subcat	Verifying Mount Points
	Using the &lit(verify mounts) subcommand causes &this to test all ningaui related 
	mountpoints defined in &lit(/etc/fstab) and all filesystems being managed by equerry.
	If any filesystem is not mounted, or the target and current states as reported by 
	equerry are not the same, the exit code will be non-zero to indicate instabiliby.
	No attempt is made to determine that the correct filesystem is mounted. 
&space
	The filesystems that are verified from &lit(/etc/fstab) are those that begin
	with the &lit(NG_ROOT) path. 
	

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-a : List values for all hosts when fetching host state variables. 
&space
&term	-e : Sends an exit command to &lit(ng_equerry.) Any positional command line parameters 
	will be ignored.
&space
&term	-F fsid : Uses fsid as the filesystem id prefix. If not supplied, &lit(rmfs) is used. This 
	allows management of filesystems that are not replication manager filesystems. If not supplied,
	&lit(rmfs) is assumed.
&space
&term	-i id : The id string used by ng_equerry on narbalek variables. In most cases this 
	option is needed only when testing a second copy of equerry.
&space
&term	-l : Long listing when fetching variable values with &lit(get.)

&space
&term	-p  port : Causes an alternate port number to be used when sending messages to 
	&lit(ng_equerry.) If not supplied, NG_EQUERRY_PORT is used.
&space
&term	-s host : Identifies an alternate system (host) to send the request to. If not 
	supplied, the localhost is assumed.
&space
&term	-v : Could cause additional chattiness for some commands.
&endterms


&space
&parms	Positional paramters are expected as <command> <data>. The following lists all 
	valid commands, the data each requires and the action that is taken.
&space
&begterms
&term	attach : Causes the named filesystem to be mounted on the local host in a read/write
	mode.  Once the filesystem is mounted, the ng_rm_db instance data is assumed to be 
	in added to the local replication manager database.  The instance data is assumed to 
	be in the &lit(NG_ROOT/site/rm) directory in a file named &lit(rm_dbd.)fsname. If an 
	alternate file of information is to be used, it may be provided as a positional parameter
	following the filesystem name on the commmand line.  The instance information is a set
	of newline separated records containing the md5 checksum value, the full pathname of 
	the file on the node, and the file size. 
&space
&term	ck_state : Check the state of the named mount point. Will write a single token
	to the standard output. The token is one of the following: mounted-rw, mounted-ro, or
	unounted.  
&space
&term	detach : Causes the named filesystem to be unmounted and the current instance list to be 
	removed from the local replication manager database.  If a target node name is supplied 
	after the filesystem name, the list is sent to that node in preperation for an attach. 
	In order to detach a file system, it must be mounted in read/write mode on the  node 
	usd to execute the command. 
&space
&term	dump_mount : &This will parse the output from the system &lit(mount) command and will 
	generate output that is consitant regardless of the O/S.  Currently this is supported 
	only for Linux, BSD and Sun. 
	Output is one line per mounted filesystem. Each line is the mountpoint and the type (rw or ro).

&space
&term	get : Looks up the value of a narbalek variable and writes it to standard output. The expected
	parameters are the type of variable (state, parms, host) and the desired filesystem. In 
	the case of fetching the state variable the host name must be supplied after the filesystem
	name, or the all option (-a) must be supplied on the command line to list the state 
	as reported by all hosts.  If the command 'get reserved' is used, then a list of 
	all reserved filesystems is written to the standard output device. 

&space
&term	list : Causes &lit(ng_equerry) to list the state of the filesystem named as the second
	command line parameter. If no parameter is given, then the state of all known fileystems
	is listed. 
&space
&term	reverify : Forces &lit(ng_equerry) to immediately reverify its perceived state with the 
	system reported state.  Usually this happens about every 3 to 5 minutes. 

&space
&term	set : Causes an equerry narbalek variable to be set.  When using the  set command 
	&this expects a variable type (host, state, or parm) as the second command line parameter. 
	The third command line parameter is expected to be the name of the filesystem that 
	the value applies to.  In the case of setting the state, the host must also be
	declared (as the fourth command line parameter).  All remaining command line tokens are 
	used as the value that is assigned to the variable. 
&space
&term	show : Accepts either &lit(host) or &lit(parm) as a paramter and displays all of the 
	equerry narbale variable values  for that type for all filesystems.  A good way to have 
	a quick peek at what is defined. 

&space
&term	verbose : Accepts the next token as the verbose level and sends a request to change the 
	&lit(ng_equerry) verbose level to the one supplied. 
&endterms

&space
&exit	An exit code that is not zero indicates an error and should be accompanied by a message
	written to the standard error device. 

&examp
	Detach the filesystem &lit(rmfs0030) and send the file instance list to the node dib97:
&litblkb
   ng_eqreq detach rmfs0030 dib97
&litblke 

&space
	Attach the filesystem &lit(rmfs0030.)
&litblkb
   ng_eqreq attach rmfs0030 
&litblke 



&space
&see	&seelink(tool:ng_equerry), &seelink(repmgr:ng_rm_db)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	02 Aug 2007 (sd) : Its beginning of time. 
&term	15 Sep 2007 (sd) : Verify mounts no longer considers noauto entries in fstab.
&term	15 Oct 2007 (sd) : Corrected spelling errors in man page.
&term	31 Jan 2007 (sd) : Corrected more spelling errors in man page.
&term	28 Feb 2008 (sd) : Now creates the mountpoint if it did not exist. Added gen command, and support for src=. 
&term	29 Feb 2008 (sd) : Calls dosu to chown the mountpoint after it is mounted. 
&term	05 Mar 2008 (sd) : Corrected typo in umount test. 
&term	26 Mar 2008 (sd) : Added ability to mark a filesystem as dropped when using the gen option.
		Updated the manual page to better describe the gen operation.
&term	07 Apr 2008 (sd) : Modified to support mounting outside of NG_ROOT and the creation of symlinks
		to the mountpoint directory from NG_ROOT.  Also verifies and creates MOUNT_POINT tags where 
		needed. 
&term	14 Apr 2008 (sd) : Added attach and detach commands to make moving repmgr filesystems easier.
&term	29 Apr 2008 (sd) : Fixed bug with gen command -- no space before mtpt= string.
		Calls start_valet to start any valets if a parking lot is mounted.
&term	02 May 2008 (sd) : Added support for !default_device_dir in gen input file. 
&term	14 May 2008 (sd) : Change to ensure rmsync is run only if mount was successful.
&term	09 Jun 2008 (sd) : Added specific function to install the fstab that can be invoked from equerry.
&term	13 Aug 2008 (sd) : Corrected man page. 
&term	03 Oct 2008 (sd) : Corrected substitution of blank for comma in flags string .** HBD 16 SMC
&term	25 Nov 2008 (sd) : Added dump_mount command to make interface with equerry process better.
&term	26 Mar 2008 (sd) : Corrected umount typo that was causing equerry not to see the correct unmounted state 
		when invoking this for ck_state.  (looks like the fix from Mar 2008 was rolled off)
&term	17 Apr 2009 (sd) : Now invokes ng_rm_arena with the pkinit command to initialise parking lot filesystems too.
		Added a success message when ng_dosu is used to create the mountpoint directory.
&term	20 Apr 2009 (sd) : Drop now clears the host narbalek variable too.
&term	22 May 2009 (sd) : Made corrections to allow everything to work on sun (vfstab).
&endterms


&scd_end
------------------------------------------------------------------ */

