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
# Mnemonic:	spy_ckfsfull 
# Abstract:	check to see if any filesystem listed on the command line is too full
#		command line parms are [threshold:]filesystem-pattern
#		
# Date:		28 June 2006
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN



# -------------------------------------------------------------------
ustring="[-I] [-i] [-p] [-v]"
def_threshold=80
inodes=""
ignore="nothing2ignore"		# pattern to ignore from ignore file
must_ignore=0			# we must read the ignore file if there
ignoref=$NG_ROOT/site/spyglass_fsfull.ignore
type="disk usage"
use_ls=0			# use ls and sum the files that match the total
pprefix="[ 	]"		#  by default find fully qualified names

while [[ $1 = -* ]]
do
	case $1 in 
		-I)	must_ignore=1;;
		-i)	inodes=-i; ignoref=$NG_ROOT/site/spyglass_fsfull_inode.ignore; type="inode usage";;
		-l)	use_ls=1; def_threshold="5G";;
		-p)	pprefix="";;

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

dff=$TMP/df.$$		#only one call per invocation goes to this file
ng_df $inodes >$dff


if (( $must_ignore > 0 )) && [[ -f $ignoref ]]
then
	read ignore <${ignoref:-/dev/null}
fi

error=0
while [[ -n $1 ]]
do
	if [[ $1 = *:* ]]
	then
		threshold="${1%%:*}"		# threshold is amount+unit when using ls, else considered a percentage
		fspat="${1##*:}"
	else
		fspat="$1"
		threshold=$def_threshold
	fi

	if [[ "$fspat" == "!"* ]]
	then
		invert="-v"
		fspat="${fspat#?}"
	else
		invert=""
	fi

	if (( $use_ls > 0 ))
	then
		ls -al $fspat | awk -v pat="$fspat" -v threshold="$threshold" '
			BEGIN { sum = 0 }
			{ sum += $5;	}
			END {
				units = 1024 * 1024;		# default to meg
				usym="M";
				if( match( threshold, "[a-zA-Z]" ) )
				{
					a[2] = substr( threshold, length(threshold), 1 );
					if( a[2] == "G" || a[2] == "g" )
						units *= 1024; 
					else
					if( a[2] == "K" || a[2] == "k" )
						units = 1024;
					else
					if( a[2] == "B" || a[2] == "b" )
						units = 1;

					usym = a[2];
				}

				if( (sum /= units) > threshold+0 )
				{
					printf( "1\n" );		# indicate error to the script
					printf( "sizes of files matching pattern \"%s\" exceed threshold: %.2f%s > %s [FAIL]\n", pat, sum, usym, threshold ) >"/dev/fd/2";
				}
				else
				{
					printf( "0\n" );		# indicate ok to the script
					printf( "sizes of files matching pattern \"%s\" less than threshold: %.2f%s < %s [OK]\n", pat, sum, usym, threshold ) >"/dev/fd/2";
				}
			}
		'| read x

		if (( ${x:-0} > 0 ))
		then
			error=1
		fi
	else
		gre $invert "$pprefix$fspat" $dff |gre "%" | gre -v "$ignore" | while read junk junk junk junk avail pct mtpt
		do
			good=1
			case $threshold in 
				*g|*G)			# if GMK suffix, then assume we want to compute based on avail amount not pctg; we assume avail is in K
					oksym=">"
					badsym="<"
					avail=$(( $avail/(1024*1024) ))
					what=$avail"G";
					if (( $avail < ${threshold%?} ))
					then
						good=0
					fi
					;;

				*m|*M)
					oksym=">"
					badsym="<"
					avail=$(( $avail/(1024) ))
					what=$avail"M";
					if (( $avail < ${threshold%?} ))
					then
						good=0
					fi
					;;

				*k|*K)
					oksym=">"
					badsym="<"
					what=$avail"K";
					if (( $avail < ${threshold%?} ))
					then
						good=0
					fi
					;;

				*)				# assume 99% or just 99
					oksym="<"
					badsym=">"
					what="${pct%%%*}%"
					if (( ${pct%%%*} > ${threshold//%/} ))
					then
						good=0
					fi
					;;
			esac

			if (( good < 1 ))
			then
				log_msg "filesystem mounted on $mtpt exceeds $type threshold: $what $badsym $threshold [FAIL]"
				error=1
			else
				log_msg "filesystem mounted on $mtpt has good $type usage: $what $oksym $threshold [OK]"
			fi
		done
	fi

	shift
done
cleanup $error



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_spy_ckfsfull:Check filesystem utilisation)

&space
&synop	ng_spy_ckfsfull [-I] [-i] [-l] [threshold[M|G|K|%]:][!]pattern...

&space
&desc	For each threshold:pattern supplied, check the output of ng_df to see if the 
	filesystem(s) that match the pattern are over the indicated utilisation.  If 
	the utilisation is omitted, 80% is assumed.  When the -i flag is given, the 
	percent of inodes used is checked. 
&space
	If &lit(ls) mode is indicated (-l), the pattern is given to the &lit(ls) command
	and the threshold is checked against the file sizes reportd. In this case the 
	threshold can designates as gigabytes, megabytes, kilobytes, or bytes by appending 
	one of the following to the threshold value: m, g, k, or b. If a trailing 
	unit definition does not exist, then megabytes is assumed. 
&space
	If the pattern starts with a bang character (!), then all filesystems that do NOT
	match the pattern are examined. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-i : Check inode usage reather than disk space consuption.
&space
&term	-I : Use the file $NG_ROOT/site/spyglass_fsfull.ignore as a list of bar (|) 
	separated patterns that when matched against the list from ng_df will be ignored.
	Easily allows a filesystem which is known to be above threshold, and that is OK, 
	to not trigger an alarm.
&space
&term	-l : Use &lit(ls) to determine the disk usage (see above).
&space
&term	-p : No prefix. By default, the pattern(s) supplied are assumed to be prefixed with
	&lit("[ \t]") such that supplying &lit(80^:/tmp) will match &lit(/tmp) and not (/ningaui/tmp).
	If this option is supplied, no prefix is used. 
&endterms


&space
&parms	The parms are one or more threshold:pattern pairs. The threshold, and separating 
	colon, may be omitted if a default value of 80% is suitible.  When using &lit(ls)
	mode (-l) the threshold is assumed to be megabytes unless the value includes a suffix character which is 
	one of: g, m, b, k.

&examp
	The following examples illustrate how to check to see if the filesystem housing
	&lit(/tmp) is over a threshold of 92%, and to see if the directory /tmp/logs
	contains more than 2 gigabytes of data.
&space
&litblkb
   ng_spy_ckfsfull 80:/tmp
   ng_spy_ckfsfull -l 2g:/tmp/logs/
&litblke



&space
&exit	A non-zero exit indicates that one or more filesystems matching the inidcated
	pattern(s) had utilisation greater than the indicated threshold.

&space
&files	These files affect the behaviour of &this.  If they exist, the first line is expected
	to be a vertical bar separated set of patterns. Filesystems matching any of the patterns
	are ignored.
&space
&begterms
&term	$NG_ROOT/site/spyglass_fsfull.ignore : Ignores these patterns when checking in regular mode. 
&space
&term	$NG_ROOT/site/spyglass_fsfull_inode.ignore : Ignores these patterns when checking inode usage. 
&endterms

&space
&see	ng_spyglass, ng_spy_ckdaemons

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	27 Jun 2006 (sd) : Its beginning of time. 
&term	09 Aug 2006 (sd) : Added -I option.
&term	26 Sep 2006 (sd) : Corrected test for ignore file.
&term	07 Dec 2007 (sd) : Added prefix and -p option to shut if off if needed. 
&term	10 Jan 2007 (sd) : Aded support for -l (ls mode) to allow vm usage on macs to be checked.
&term	17 Jan 2007 (sd) : Added support for !pattern and updated man page to reflect the files
	that affect processing.
&term	29 Sep 2008 (sd) : Added ability to alarm based on hard amount (e.g. 500M) when examining a filesystem using ng_df.
&term	21 Apr 2009 (sd) : Corrected man page.
&endlist


&scd_end
------------------------------------------------------------------ */
