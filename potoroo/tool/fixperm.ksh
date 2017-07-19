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
#!/ningaui/bin/ksh
# Mnemonic:	fixperm.ksh
# Abstract:	ensures that files in the requested subdir have the desired permissions
# Date:		6 May 2002
# Author: 	E. Scott Daniels
# --------------------------------------------------------------------------------------

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

# --------------------------------------------------------------------
ustring="[-a] [-d directory] [-i] [-n] [-p perms] [-v]"
verbose=0
target=775			# target permissions
type="-type d"			# default to checking only directories
forreal=1			# -n turns off and we will just say what we'd like to do
dlist=""			# directories to check; defaults to repmgr area if -d not supplied
log=$NG_ROOT/site/log/fixperm
interactive=0;			# by default we assume that its being run from rota or somesuch


while [[ $1 = -* ]]
do
	case $1 in 
		-a)	type="";;			# all files

		-A)	age="-mtime +$2"; shift;;

		-d)	dlist="$dlist $2"; shift;;
		-d*)	dlist="$dlist ${1#-?}";;

		-i)	interactive=1;;

		-n)	forreal=0;;

		-p)	target="$2"; shift;;
		-p*)	target="${1#-?}";;

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

#if [[ `id -nu` != "ningaui" ]]
if ! amIreally ${NG_PROD_ID}
then
	log_msg "you are not ningaui; will only list whats wrong"
	forreal=0
fi

if (( $interactive <= 0 ))
then
	exec >>$log 2>&1
fi


if [[ -z $dlist ]]		# user did not supply; default to repmgr
then
	for x in $NG_ROOT/site/arena/*
	do
		dlist="$dlist `head -1 $x`"
	done
fi


verbose "fixing permssions ($target) in $dlist"

>$TMP/PID$$.fix
for x in $dlist
do
	/usr/bin/find $x $age $type ! -perm $target    #>> $TMP/PID$$.fix 2>/dev/null
done | awk -v verbose=${verbose:-0} '
	{
		x = split( $1, a, "/" );
		if( index( $1, "/." ) || substr( a[1], 1, 1 ) == "." || a[x] == "lost+found" )
		{
			if( verbose )
				printf( "ignored: %s\n", $0 ) >"/dev/fd/2";
			next;
		}

		print;
	}
' >$TMP/PID$$.fix

if [[ ! -s $TMP/PID$$.fix ]]
then
	log_msg "nothing found to fix"
	cleanup 0
else
	c=`wc -l < $TMP/PID$$.fix`
fi

if [[ $forreal -gt 0 ]]
then
	if (( $interactive <= 0 ))
	then
		echo "------------ `date` -----------------" >&2
	fi

	log_msg "$c files found to fix"
	xargs chmod $target <$TMP/PID$$.fix 2> $TMP/errors.$$
	wc -l < $TMP/errors.$$ | read count
	if [[ $count -gt 0 ]]
	then
		cat $TMP/errors.$$ >>$log
		if (( $interactive > 0 ))
		then
			log_msg "$count messages were generated while attempting to fix permissions; see $log"
		else
			ng_log $PRI_WARN $argv0 "$count messages were generated while attempting to fix permissions; see $log"
		fi
	else
		log_msg "all permissions changed without error"
	fi
else
	echo "no execution mode: $c files would be fixed:"
	xargs ls -ld <$TMP/PID$$.fix
fi

cleanup 0

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_fixperm:Ensure permissions correct for files)

&space
&synop	ng_fixperm [-a] [-A days] [-d directory] [-n] [-p perms] [-v]

&space
&desc	&This will check each directory listed with a &lit(-d) option to ensure that 
	all sub directories (and files if &lit(-a) option provided) have the 
	desired permissions (775 unless the &lit(-p) option is used. Using the 
	&lit(-n) option will cause no action to be taken other than to list the 
	directories (and possibly the files) that would have their permissions 
	changed.

&space
	All files and directories that begin with a dot (.) are ignored.

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-a : Check all files in addition to directories. By default, only directory permissions
	are checked. 
&space
&term	-A days : Causes only files older than &ital(days) to be considred.  Files that 
	have modification times that are more recent than &(days) will be ignored. &This 
	uses &lit(find) to search for files, and different implementations of the &lit(find) 
	command have, in the past, yielded differing results for the '-mtime +x' parameter, 
	so your results may vary. 
&space
&term	-d dir : Specifiy a driectory to check.  Multiple &lit(-d) options can be 
	provided.  If no directory names are provided on the command line, &this 
	defaults to checking the &ital(replication manager) directories as listed
	in the &lit(NG_ROOT/site/arena) directory.
&space
&term 	-n : No action; just list what would be done.
&space
&term	-p perms : Specify the target permissions that all directories should have
	(e.g. 750).
&space
&term	-v : Verbose mode.
&endterms


&space
&see	&ital(ng_repmgr) &ital(ng_spaceman) 

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 	06 May 2002 (sd) : Original code.
&term	16 Jan 2005 (sd) : Changed log message to a warning.
&term	15 Oct 2007 (sd) : Modified to allow use of NG_PROD_ID rather than hardcoded ningaui.
&term	14 Apr 2008 (sd) : Added code to ignore files beginning with a dot (.).
&term	29 Apr 2008 (sd) : Fixed problem where /.dir was being ignored but not /.dir/file.
&endterms

&scd_end
------------------------------------------------------------------ */
