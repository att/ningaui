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

# Mnemonic:     ng_fix_mlog_unattached
# Abstract:     Register unattached mlog and masterlog instances (part 1)
#
# Date:         26 June 2006
# Author:       Bethany Robinson

# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# --------------------------------------------------------------------

set -o pipefail

NumDays=3
NumLogs=200

ustring="[-d num_days] [-l num_logs] [-n]"
verbose=0
forreal=1
hood_only=0	# -h sets and causes us to exit after setting hoods

while [[ $1 = -* ]]
do
        case $1 in
                --man)  show_man; cleanup 1;;
		-h)	hood_only=1;;
                -n)     forreal=0;;
                -d)     NumDays=$2; shift;;
                -l)     NumLogs=$2; shift;;

		-v)     verbose=$(( $verbose + 1 ));;
                -\?)    usage; cleanup 1;;
                -*)     error_msg "unrecognised option: $1"
                        usage;
                        cleanup 1
                        ;;
        esac

        shift
done

# only run on the Repmgr node
node="$(ng_sysname)"
repmgr=${srv_Filer:-no-filer}
if [[ $node != $repmgr ]]
then
        log_msg "must run on Filer which seems to be: $repmgr"
        cleanup 1
fi


# first suss out any files in repmgr that have lost their hoods
verbose "checking for mlog files without hoods"
rmd=$NG_ROOT/site/rm/dump
hfile=$TMP/PID$$.hfile
gre "^file.*mlog" $rmd | gre -v "hood=" | awk '
	{
		gsub( ":", "", $2 );			# strip the trailing : from the filename 
		x = split( $2, a, "." );		# suss off the mlog date (a[x])
		split( $3, b, "=" );			# snag the md5 value (b[2])

		printf( "hood %s %s MASTERLOG_%s\n", b[2], $2, a[x] );		# repmgr hood cmd
	}
' >$hfile

wc -l <$hfile | read count
if (( $count > 0 ))
then
	if (( $forreal > 0 ))
	then
		verbose "adding hoods to $count files [OK]"
		ng_rmreq <$hfile
	else
		verbose "no action set, would fix hoods on $count files"
	fi
else
	verbose "all mlog files have hoods [OK]"
fi
	
if (( $hood_only > 0 ))
then
	verbose "hood only set -- not attempting to process unattached instances"
	cleanup 0
fi


# =========== now go after and cleanup any unattached files ====================

wd=$TMP/fixmlogs.$$			# create a working directory
mkdir $wd 2>/dev/null
if ! cd  $wd
then
	echo "Cannot switch to $wd"
	cleanup 1
fi

# find the unattached mlog and masterlog instances
joinf=$TMP/join.$$
ng_rm_join < $NG_ROOT/site/rm/dump > $joinf

# get the unattached instances
awk '
        /unattached/ {
                go=1;
                next;
        }

        {
                if(go)
                {
                        x = split( $0, a, "#" );
                        for( i =1; i <= x; i++ )
                                print a[i];
                }
        }

        END {
        }
' <$joinf | grep -v "\*\*deleted\*\*"  > temp1

# Only process 200 unattached mlogs
gre "\/mlog\." temp1 | sed ${NumLogs}q >temp_mlog


# Only process 3 days worth of unattached masterlogs 
gre "\/masterlog\." temp1 > temp2
for i in `cat temp2 | awk '{print $2}' | awk -F\. '{print $2}' | sort -u | sed ${NumDays}q`
do
	gre $i temp2 
done >temp_masterlog


# Generate woomera commands for each node
cat temp_mlog temp_masterlog | awk '
	{
		fn = sprintf( "cmd.%s.%d", $1, idx[$1]+0 )
		printf( "%s : RM_FIXMLOGS cmd ng_fix_mlog_unattached2 -v %s %s %s >>$NG_ROOT/site/log/fix_mlog2 2>&1\n", fn, $2, $3, $4 );
		if( n[$1]++ > 200 )
		{
			n[$1] = 0;
			idx[$1]++;
		}
	}
' | sort | awk ' 
	{
		if( last && last != $1 )
		{
			#printf( "closing %s\n", last ) >"/dev/fd/2";
			close( last );
		}
		last = $1;
		$1 = "job" ;
		printf( "%s\n", $0, last ) >last;
	}
'

if (( $forreal == 0 ))
then
	cond_msg="would "
fi

if (( $forreal > 0 || $verbose > 0 ))
then
	wc $wd/* >&2
fi

# Execute woomera commands
for x in $(ls cmd* 2>/dev/null )
do
	host=${x%.*}
	host=${host#*.}

	log_msg "${cond_msg}submit commands: $host: $x"
	if [[ $forreal -gt 0 ]]
	then
		ng_wreq -s $host <$x
	fi
	if (( $forreal > 0 || $verbose > 1 ))
	then
		cat $x >&2
	fi
done

/bin/rm -rf $wd


cleanup 0
# should never get here, but is required for SCD
exit

#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start

&doc_title(ng_fix_mlog_unattached:Register unattached mlog and masterlog instances)

&space
&synop  ng_fix_mlog_unattached [-d num_days] [-h] [-l num_logs] [-n] [-v]

&space
&desc   &This is a repair program run daily out of &ital(rota).
	It grubs through the repmgr dump looking for unattached mlog and masterlog files with 
	the intent of recycling them back into the log_comb process.  For each file it wants to 
	recycle, &this schedules a woomera job on the node where the file resides.  
&space
	In addition, mlog files without hoods are searced for and hoods assigned to these
	files.  If an mlog file is not hooded to the correct node it will never be picked up 
	and added to the masterlog file.  

&space
&opts   The following options are recognised by &this when placed on the command line:
&begterms
&term   -d numdays : If too many days of masterlog files are released at one time, 
	$TMP will fill up.  This option specifies how many days should be released.
	The default is 3.
&spae
&term	-h : Hood only.  If set, causs the programme to set hoods on mlog files and then 
	stop (no attempt to correct unattached instances is made).
&space
&term   -l numlogs : This option limits the number of mlogs being injected back into
	the system.  A sudden influx of lots of unattached mlogs usually means something
	is broken with repmgr.  By default, only 200 mlogs are recycled at one time.

&space
&term   -n : No execution mode.  Goes through the motions but does not schedule
        &ital(woomera) jobs.
&space
&term	-v : Verbose mode. Using two -v options lists the commands that would be sent when in 
	no execution mode. 
&space
&endterms

&space
&see  ng_fix_mlog_unattached2
&owner(Bethany Robinson)
&beglgterms
&term	03 Feb 2007 (sd) : Small changes to write commands being executed to stderr, and to 
	fix an error if there were no masterlog files found to recycle. 
&space
&term	10 Mar 2009 (sd) : Added the search for mlog files without hoods. 
&endterms
&scd_end

