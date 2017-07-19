#!/usr/common/ast/bin/ksh
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


#!/usr/common/ast/bin/ksh
# Mnemonic:	ng_cram
# Abstract:	[un]compress file(s). by default we uncompress! 
#		
# Date:		26 Aug 2005 
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

function do_stats
{
	awk -v m="$3" -v start=$1 -v stop=$2 '	
	function s2dhms( tsec, 		d, h, m, s )		# seconds to dayhourminutesec
	{
		if( tsec < 0 )
			return " ";		

		tsec /= 10;			# convert to seconds
		d = int(tsec / 86400);
		if( d > 180 || d < 0 )
			return " ";		# likely a missing lead date

		tsec -= d * 86400;
		h = int(tsec / 3600);
		tsec -= h * 3600;
		m = int(tsec / 60);
		tsec -= m * 60; 

		if( d )
			return sprintf( "%dd%02dh%02dm%02ds", d, h, m, tsec );

		if( h )
			return sprintf( "%02dh%02dm%02ds", h, m, tsec );
		else
			return sprintf( "%02dm%02ds", m, tsec );
	}

	BEGIN {
		printf( "elapsed time for %s was %s\n", m, s2dhms(stop - start) );	
	}
	'|read message

	verbose "$message"
}

function set_opts
{
	opts=""

	case $method in 
	vczip)
		# if we only support -tv -t -tnl or -qv then we should verify, but if we support -C anything we cannot verify
		#if (( $scatter == 0 ))		# decompress
		#then
		#	case "$vcopts" in
		#		t|tv|tnl|qv)		opts="$opts-$vcopts ";;
		#		-t|-tv|-tnl|-qv)	opts="$opts$vcopts ";;
		#		"")	;;
#	
#				*)	log_msg "invalid option for vczip: $vcopts"
#					log_msg "legit vczip opts: t, tv, tnl, qv"
#					cleanup 1
#					;;
#			esac
#		fi

		if (( $scatter == 0 ))		# there must be some vczip option set 
		then
			if [[ -z $vcopts ]]
			then
				log_msg "in compress mode (-r) compression options must be supplied with -c or -C"
				cleanup 1
			fi
		else
			vcopts="-u";
		fi

		opts="$opts$vcopts "		
		;;

	*pzip*)	opts="$opts$popts ";;		# we have no way to verify

	gzip)
		if (( $scatter > 0 ))		# blow the bits to the wind
		then
			opts="$opts-d -c "		
		else
			case $glevel in 
				[1-9]) 	opts="$opts-$glevel ";;
				*)	log_msg "level supplied on -gn is invalid: $glevel"
					cleanup 1
					;;
			esac
		fi

		if (( $force > 0 ))
		then
			opts="$opts-f "
		fi

		opts="$opts-n "		# keeps gzip from putting date stamp into compressed file
		;;
	
	*) 	;;
	esac
}

# given fname suffix as parameters make an output name based on whether we are squishing or scattering the bits. 
# gzip returns nothing as it deals with it automagically
function mkname
{
	typeset nname=""

	if (( $overwrite == 0 ))
	then
		echo "/dev/fd/1"
		return
	fi

	if (( scatter > 0 ))	# decompress turn x.pz into x
	then
		case $2 in
			pz) 	nname="${1%.pz}";;
			gz) 	nname="${1%.gz}";;
			vz)	nname="${1%.vz}";;
			*)	log_msg "internal error in mkname: dont know what to do with fn=$1 suffix=$2"
				cleanup 1
				;;
		esac
	else
		case $2 in
			pz) 	nname="${1}.pz";;
			gz) 	nname="${1}.gz";;
			vz)	nname="${1}.vz";;
			*)	log_msg "internal error in mkname: dont know what to do with fn=$1 suffix=$2"
				cleanup 1
				;;
		esac
	fi

	if (( $force == 0 ))  &&  [[ -f $nname ]]
	then
		log_msg "output file ($nname) exists, remove it, or use -f to overwrite"
		cleanup 1
	fi

	echo $nname
}

# determine method from file name
function suss_method
{
	case $1 in 
		*.pz)	echo $pzip_cmd;;
		*.gz)	echo gzip;;
		*.vz)	echo vczip;;
		*)	log_msg "cannot determine decompress method from filename: $1"
			log_msg "use -c|-p|-g to select the method"
			cleanup 1
			;;
	esac
}


function usage
{
	cat <<endKat
usage:	
	compress:
	$argv0 {-c {t|tv|tn|qv} |-C vczip-options | -g[n] | -p pinfile} [-n] [-o] [-v]   -r   [file...]

	uncompress:
	$argv0 {-c | -g | -p} [-n] [-v]  [file...]

endKat
}

# -------------------------------------------------------------------
glevel=5
method=""
vcopts=""
overwrite=0
scatter=1			# by default we uncram things
opts=""				# options passed when method is invoked
forreal=1
missing_method=0		# if user does not give us a method, we might be able to determine it
force=0

if whence ng_pzip >/dev/null 2>&1
then
	pzip_cmd=ng_pzip		# special pzip wrapper on some systems
else
	pzip_cmd=pzip
fi

while [[ $1 = -* ]]
do
	case $1 in 
		-c)	method=vczip; 			# assume it might be something like -c tv  or just -c (needed if input is stdin)
			scatter=0
			case $2 in 
				t|tv|tnl|qv)		vcopts="$vcopts-$2 "; shift;;
				-t|-tv|-tnl|-qv)	vcopts="$vcopts$v2 "; shift;;
				*)			scatter=1;;			# no option; assume decompress	
			esac
			;;
		-c*)	method=vczip;
			scatter=0
			vcopts=${1#-c}		
			;;
		-C)	vcopts="$vcopts$2 "; shift	# anything they enter we pass in as is - no verification; assume compress
			scatter=0
			method=vczip
			;;

		-f)	force=1;;
			
		-g)	method=gzip;;
		-g*)	method=gzip; glevel="${1#-g}";;			# save as glevel to verify

		-n)	forreal=0;;

		-o)	overwrite=1;;

		-p)	method=$pzip_cmd; 			# assume -p xxx  where xxx could be a filename or a pin file name
			if [[ -n $2 ]]
			then
				case $2 in 
					-*)	;;	# assume -p by itself (scatter mode)
					*)	if ! [[ -e $2 ]]
						then
							popts="$popts-p $2 " 	# assume if it is not a file then its a pin file
							shift
						fi
						;;
				esac
			fi
			;;
		-p*)	method=$pzip_cmd; popts="$popts-p ${1#-p} "; shift;;	

		-r)	scatter=0;;					# ram it rather than scatter it (compress)


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



if [[ -z $method ]]		# set flag to determine method based on filename(s)
then
	missing_method=1
fi

if [[ -z $1 ]]				# no filename listed, so silently ignore overwrite, assume input on sdin  and write to stdout 
then
	ng_dt -i |read start_ts
	set_opts
	$method $opts 
	rc=$?
	ng_dt -i |read stop_ts
	do_stats $start_ts $stop_ts "$method ($opts)"
	
else
	while [[ -n $1 ]]
	do
		if ! [[ -r $1 ]]
		then
			log_msg "file not found/readable: $1"
			cleanup 1
		fi

		if (( $missing_method  > 0 ))
		then
			if ! suss_method $1 | read method
			then
				cleanup 1
			fi
		fi

		set_opts		 # validate things  and set options for the current method

		if ! case $method in 
			vczip)	output=$(mkname $1 vz);;
			gzip) 	output=$(mkname $1 gz);;
			*pzip*) 	output=$(mkname $1 pz);;
			*)	output="";
		esac
		then
			# mkname generates an appropriate error message if it fails; we just need to clean up here
			cleanup 3
		fi

		if [[ $1 == $output ]]
		then
			log_msg " error: input and output names are the same: $1 $output"
			cleanup 2
		fi

		if (( $forreal > 0 ))
		then
			ng_dt -i | read start_ts
			if ! $method $opts <$1 >$output
			then
				if (( $overwrite > 0 ))
				then
					rm -f $output		# ditch the corrupted output file
				fi
				log_msg "error: $method failed for $1"
				rc=1
			else
				ng_dt -i | read stop_ts
				if (( $overwrite > 0 ))		# we created $output ok, ditch input file if overwriting
				then
					rm -f $1
				fi

				do_stats $start_ts $stop_ts "$method ($opts)"
			fi
		else
			log_msg "would run: $method $opts <$1 >$output"
		fi
		shift
	done
fi
cleanup $rc



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_cram:Decompress/compression tool)

&space
&synop
&litblkb
	ng_cram {-c {t|tv|tnl|qv} | -C vczip-options | -g[n] | -p pinfile} [-n] [-o] [-v]   -r   [file...]
	ng_cram {-c | -g | -p} [-n] [-o] [-v]  [file...]
&litblke

&space
&desc	&This is a wrapper for &lit(vczip, pzip,) and &lit(gzip.) By default &this 
	&stress(DECOMPRESSES) the file(s) supplied on the command line, or via standard
	input device to the standard output device. If multiple files are supplied on 
	the command line, the decomression is based on the final suffix of the file
	(.gz, ^.pz, or ^.vz).  
&space
	In order to compress a file the -r (ram) option must be supplied along with one 
	of the method specifier options (-p, -c, or -g).  The method options may 
	require a following token which is passed as a parameter to the programme 
	used to compress the input.  
&space
	Output is written to the standard output device unless the overwrite
	(-o) option is given. When in overwrite mode, the output file is written to 
	disk using the input file name and adding/removing the appropriate suffix
	(e.g. scooter.vz is decompressed into scooter). Overwrite mode causes the 
	file being read to be removed after a successful [de]compress.

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-c ^[arg] : Select &lit(vczip) mode.  When compressing one of the following arguments
	must follow the &lit(-c): [-]t, [-]tv, [-]tnl, [-]qv. These coorespond to the command line options
	to &lit(vczip); the &lit(vczip) manual page contains information as to the meaning of 
	each. This option can be supplied without an argument and must be when a vczipped file 
	is supplied for decompression on standard input. 
&space
&term	-C vczip-arg : Allows any of the non-standard arguments to be passed directly to &lit(vczip)
	for compression.  The argument(s) supplied here are not validated. 
&space
&term	-g[n] : Select &lit(gzip) mode.  Optionally the compression level may be appended to 
	the option letter (e.g. -g4).  
&space
&term	-p ^[pin-file] : Select &lit(pzip) mode.  When compressing the pin file must be supplied as 
	the token immediately following the -p.  If decompressing this option is needed only if 
	the input file does not have a trailing ^.pz suffix, or is being supplied on the standard
	input. 
&space
&term	-r : Ram mode (compress). By default, &this assumes decompression.  This causes the 
	appropriate options to be set for compression. 
&space
&term	-n : No execute mode. Will list what it might do. 
&space
&term	-o : Overwrite mode.   The original file is removed after it is successfully compressed or decompressed. 
	Output is written to disk, rather than standard output, and named accordingly.
&space
&term	-v : Verbose mode. 
&endterms


&space
&parms	&This expects positional parameters to be supplied on the command line. These are the file(s) to be operated 
	on.  When in decompression mode (default), the file suffix (.gz, ^.pz, ^.vz) is used to determine the 
	tool that is invoked (gzip, pzip, or vczip).  If there are no files supplied on the command line, 
	the standard input device is assumed, and the method must be specifically indicated with one of 
	&lit(-p, -c) or &lit(-g) options.  

&space
&exit
	An exit code that is not zero indicates an error.  &This will exit with an error if it finds a problem
	before invoking the compression tool.  Once the compression tool is invoked, the result of the 
	tool is returned. 
&space
&examp
&litblkb
Decompression
   ng_cram x.gz			# decompress the file to stdout
   ng_cram -o x.gz      	# decompress the file and leave in x
   cat x.gz |ng_cram  -g    	# decompress x.gz to stdout

Compression 
   ng_cram -p ama ama-file >ama-file.gz	 	# compress an ama file using pzip
   ng_cram -g -o ama-file			# remove ama-file after ama-file.gz ok
   ng_cram -c tv file >file.vz
   ng_cram -c tv -o file 			# same result as previous, but file is delted
   
&litblke

&space
&see	vczip, gzip, pzip

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	27 Aug 2005 (sd) : Sunrise.
&term	31 Aug 2005 (sd) : To properly set ops when no files on command line.
&term	13 Sep 2005 (sd) : Force on a -w option when invoking vczip.
&term	23 Sep 2005 (sd) : Removed -w option for vczip as the problem in vczip was fixed.
&term	30 Sep 2005 (sd) : Prevent attempt to remove /dev/fd/1. 
&term	27 Feb 2006 (sd) : Gzip now sets -n option to keep it from putting the timestamp into 
		the compressed file.  This allows two compressions of the same file (at the same 
		level) to have identical MD5sums.  (hbd kak)
&term	09 May 2006 (sd) : Added elapsed time output if in verbose mode (bsr request).
&term	21 Nov 2008 (sd) : Corrected formatting issue in man page. 
&endterms

&scd_end
------------------------------------------------------------------ */
