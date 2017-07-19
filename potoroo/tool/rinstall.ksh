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
# Mnemonic:	ng_rinstall
# Abstract:	installs a file onto another system in the cluster
# Date:		10 May 2002
# Author:	Scott Daniels
# ------------------------------------------------------------------------

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

function usage
{
	cat <<endcat
	usage:	$argv0 [-g] [-n] src-file destination-directory host1 [host2 ... hostn]
	      	$argv0 [-g] [-n] -a src-file destination-directory 
	      	$argv0 [-g] [-n] -b src-file host1 [host2 ... hostn]
	      	$argv0 [-g] [-n] -a -b src-file 

	-a : install on all nodes in the cluster
	-b : install in \$NG_ROOT/bin
	-g : add ng_ prefix to src-file name when making dest filename
	-n : no action, just list what would be done
endcat
}

myhost=`ng_sysname`
add_ng=""
verbose=0
just_kidding=0
all=0
vflag=""
ddir=""
ustring="[-g] src-file destination_dir host1 [host2...hostn]"


while [[ $1 == -* ]]
do
	case $1 in 
		-a)	all=1;;					# install on all hosts in cluster 
		-b)	intobin=1;;				# install to bin; parms are src [host...]
		-g) add_ng="-g" ;;				# passes this flag when we run ng_install on remote machine
		-n) just_kidding=1;;
   		-v) verbose=1; vflag="-v";;
   		-man|--man) show_man; exit;;
   		*)	usage
			exit 1 
			;;
	esac

	shift
done

if [[ ! -f $1 ]]
then
	echo "$argv0: giving up: cannot find file to install: $1"
	cleanup 1
fi

src=$1
shift

if [[ $intobin -lt 1 ]]			# indicates that next parm is the dest info 
then
	ddir=$1
	shift
fi


if [[ $all -gt 0 ]]
then
	set `ng_members -n`		# list of all nodes
	verbose "all selected, targets are: $@"
else
	if [[ -z $1 ]]
	then
		log_msg "missing parm: destination host(s)"
		usage
		cleanup 1
	fi
fi


tmpf=/tmp/${src##*/}						# important to keep src name as extensions .ksh are treated by install
verbose "command = ng_install $add_ng $tmpf $ddir"

while [[ -n $1 ]]
do
	if [[ $1 != $myhost ]]
	then
		dhost=$1
		log_msg "running on $dhost:"
		if [[ just_kidding -lt 1 ]]
		then
			if ng_ccp -d $tmpf $dhost $src 2>/tmp/err.$$
			then
				ng_rcmd $dhost ng_install $vflag $add_ng $tmpf $ddir
				ng_rcmd -f $dhost rm $tmpf
			else
				log_msg "could not ccp $src to $dhost"
				cat /tmp/err.$$
				cleanup 1
			fi
		else
			log_msg "-n option set; no action taken to install $src"
		fi
	fi

	shift
done

cleanup 0



exit  # should never get here, but is required for SCD

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&doc_title(ng_rinstall:Install an executable on remote node)

&space
&synop	ng_rinstall [-g] [-n] src-file destination host1 [host2 ... hostn]
	&break ng_rinstall [-g] [-n] -a src-file destination
	&break ng_rinstall [-g] [-n] -b src-file host1 [host2 ... hostn]
	&break ng_rinstall [-g] [-n] -a -b src-file 
&space
&desc	&This installs the &ital(source-filename) into 
	the file or directory specified as &ital(desitination)  on the 
	remote nodes listed.
	The destination can be defaulted to the Ningaui &lit(bin) directory
	by using the &lit(-b) option and omitting the destination  parameter.
	If the source file has a &lit(.ksh) extension, then this extension is
	removed. 
&space
	For each host (or all hosts if &lit(-a) is supplied as an option)
	the source file is coppied to the remote node, and the 
	&lit(ng_install) script is executed on the remote node to install
	the file. 

&space
&opts	The following options are recognised:
&begterms
&term	-a : install on all nodes in the cluster (as determined by the 
	&lit(ng_member) command). Any host names supplied as parameters
	will be ignored. 
&space
&term	-b : install in $NG_ROOT/bin on each remote node. (destination 
	parameter should be omitted or will be considered a host name).
&space
&term	-g : add ng_ prefix to src-file name when making dest filename.
&space
&term	-n : no action, just list what would be done.
&endterms

&space
&parms  The following parameters are recognised when entered on 
	the command line. The parameters must be entered in the order
	listed below:
&space
&begterms
&term	source-filename : The name of the source file to be installed.
&space
&term	destination : The name of the destination file or destination 
	directory. This parameter must be omitted if the &lit(-b) option 
	is used. 
&term	host(s) : A list of host names to which the source file should be
	installed.
&endterms

&space
&examp	To install the script ng_aardvark.ksh into the &lit(/ningaui/bin)
	directory one of the following commands can be used:
&space
&litblkb
	ng_rinstall ng_aardvark.ksh /ningaui/bin/ng_aardvark ning00
	ng_rinstall ng_aardvark.ksh /ningaui/bin ning00
	ng_rinstall -b ng_aardvark.ksh ning00 ning01 ning03
	ng_rinstall -a -b ng_aardvark.ksh 		# on all hosts
&litblke
&space
	To install .config into the /ningaui directory one of the following 
	commands can be used:
&litblkb
	ng_rinstall .config /ningaui/.config ning00
	ng_rinstall -a .config /ningaui
&litblke

&see	&ital(ng_install) &ital(ng_ccp)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	10 May 2002 (sd) :  First try.
&endterms
&scd_end
