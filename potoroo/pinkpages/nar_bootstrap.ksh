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
# Mnemonic:	nar_bootstrap
# Abstract:	we pretend to be another narbalek and 'dump in' a chkpt file to the 
#		narbalek indicated on the command line.  The information put in WILL
#		be propigated to all narbaleks in the tree!
#		
# Date:		22 Sep 2004
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# -------------------------------------------------------------------
ustring=" [-f init-file] [-n] [-p port] [-s host] [-v]"

host=localhost
port=${NG_NARBALEK_PORT:-4567}
really_send=1
file=/dev/fd/0

while [[ $1 = -* ]]
do
	case $1 in 
		-f)	file=$2; shift;;
		-n)	really_send=0;;
		-s)	host="$2"; shift;;
		-p)	port="$2"; shift;;

		-v)	vflag="-v$vflag "; verbose=1;;
		-x)	;;				# ignore; its for debugging
		--man)	show_man; cleanup 1;;
		-\?)	usage; cleanup 1;;
		-*)	error_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done


awk '
	substr( $1, 1, 1 ) == "#" { next; }	# ckpt will not have comments, but an init file might

	{
		rest = substr( $0, 2 );
		if( (c = substr( $1, 1, 1 )) == "%" )			# comment 
			printf( "C %s\n", rest );
		else
			if( c == "@" )
				printf( "S %s\n", rest );
		next;
	}

	END{
		printf( "D\n" );
	}
	
' <$file | (
	if (( $really_send > 0 ))
	then
		ng_sendtcp -d $vflag -e "#end#" $host:$port >/dev/null		# -d causes all data to dump to narbalek before listen for end str
	else
		cat
	fi
)

cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_nar_bootstrap:Bootstrap values into Narbaekek)

&space
&synop	ng_nar_bootstrap [-f filename] [-p port] [-n] [-s host] [-v]

&space
&desc	Converts a narbalek checkpoint file into proper messages and sends 
	the messages to the narbalek on the local node. The file may be 
	supplied as standard input, or as an existing file using the -f option.
	The primary use of &this is to initialise narbalek with a standard set 
	of variable values on a new cluster, but also can be used to restore 
	values from a checkpoint file ($NG_ROOT/site/narbalek/*). 

&space	
	Information that is loaded with &this will be propigated to all other
	narbalek processes that are currently attached to the communication tree. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-f file : Supplies an alternate initialisation file. If not supplied the 
	&lit(narbalek.init) file in the site directory is used. 
&space
&term	-p port : Port that the target narbalek is listening on. If not used, 
	&lit(NG_NARBALEK_PORT) is assumed. 
&space
&term	-n : No send mode.  &This will echo the messages to the standard output
	that would have been sent to narbalek.
&space
&term	-s host : The name of the host where narbalek is running. If not supplied, 
	the local host is assumed. 
&space
&term	-v : Verbose mode. 
&endterms

&space
&exit 	A return code that is not zero indicates an error.

&space
&see	ng_narbalek, ng_pp_build, ng_ppset, ng_ppget, ng_nar_set, ng_nar_get.

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	22 Sep 2004 (sd) : The first attempt.
&term	24 Aug 2007 (sd) : Corrected man page and added -n option. 
&term	16 Oct 2007 (sd) : Corrcted typo in really-send check.
&endterms

&scd_end
------------------------------------------------------------------ */
