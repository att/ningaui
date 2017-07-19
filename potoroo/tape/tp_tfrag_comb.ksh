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


CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}	# get standard configuration file
. $CARTULARY 

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}	# get standard functions
. $STDFUN


set -x

tmp=$TMP/tp_comb_$$
me=`ng_sysname`
nomail=false	# always send mail on failure (-n option not implemented)

solo

function sendit
{
        note=$TMP/tfragcomb_mail.$(ng_dt -i)
        rm -f $note

        node=`ng_sysname`
        
	case "$1" in
	1)	msg="tp_tfrag_comb failed on $node (ERK)" 
		echo "$msg">> $note
		;;
	2)	msg="tp_tfrag_comb failed on $node (FINISH)" 
		echo "$msg">> $note
		;;
	*)	msg="tp_tfrag_comb failed on $node" 
		echo "$msg">> $note
		;;
	esac

	echo >> $note
	echo "To remove the goaway file:" >> $note
	echo "	either run ng_tp_tfrag_comb interactively" >> $note
	echo "	or rm $errfile" >> $note

        #Mail -s "tp_tfrag_comb failed on $node" ningaui@research.att.com < $note
        #rm -f $note
	ng_log $PRI_ALERT $argv0 "@NG_TAPE_MAIL $msg (see $note for details)"
}

finish(){
	ng_rm_db -d < $1.2
	ng_flist_send
	# the 999 is just a dummy size; it doesn't matter
	sed 's/.*/file & 999 0/' < $1.2 | ng_rmreq
	awk '{print $2}' < $1.2 | xargs rm -f
	cp $1.2 /tmp/log_gone
	rm -f $1.* $1-* $errfile
}

mysort(){
	sort -T $TMP "$@"
}
sortmlog(){
	mysort -t. +2 "$@"
}
badlines(){
	gre -s -v '^[^ ][^ ][^ ][^ ][^ ][^ ][^ ][^ ][^ ][^ ][^ ][^ ][^ ][^ ][^ ][^ ][^ ][^ ][^ ][^ ][^ ][^ ][^ ][^ ][^ ][^ ][^ ][^ ][^ ][^ ][^ ][^ ] ' $1 > /dev/null
}

# ----------------------------------------------------------------------------
filer=${srv_Filer:-no-filer}
logfile=$NG_ROOT/site/log/tcomb
errfile=$logfile.err
batch=0

while [[ $1 = -* ]]
do
	case $1 in 
		-r)	finish $2; exit;;
		-x)	batch=1;;

		-v)	verbose=1;;
		--man)	set +x; show_man; cleanup 1;;
		-\?)	set +x; usage; cleanup 1;;

		-*)	set +x; 
			log_msg "unrecognised option: $1"
			usage;
			cleanup 1
			;;
	esac

	shift
done

if [[ $batch = 1 ]]
then
	if [[ -s $errfile ]]
	then
		echo $err nonzero size
		exit 1
	fi
	exec > $logfile 2>&1
fi

#1) get a list of frag filess in $list (on current filer node)
#2) cat $list | split_into_weekly (names in $weeks)
#3) for each file $f in $weeks, get week file for $f, add $f to it, and rename to week file for $f
#4) set noccur for $list to 0

snag(){
	if grep -s "not found: $1" $tmp.master
	then
		if $NG_ROOT/bin/ng_spaceman_nm -t $1 > $tmp.8
		then
			inst=`cat $tmp.8`
			gzip < /dev/null > $inst
			return 0
		else
			echo "Snag error:  ng_spaceman_nm failed"
			return 1
		fi
	fi
	sed -n "/#$me /s/.*#$me //p" $tmp.master | sed 's/#.*//' | grep "/$1\$" > $tmp.xx
	if test -s $tmp.xx
	then
		inst=`cat $tmp.xx`
		return 0
	else
		echo "Snag error: no instance of $1 on this system; try again later"
		ng_log $PRI_WARN $0 "$1 not on this node; try again later"
		rm -f $tmp.*
		exit 0
		return 1
	fi
}
	
# get a list of tfrags
ng_rm_db -p | gre /pf-tfrag | sed 's:.*/:z:; s/ .*//; s/z/dump1 /' > $tmp.2
ng_rmreq -a -s $filer < $tmp.2 | grep "pf-tfrag-.*matched=1.*#"$me |
	sed 's/.*md5=\([^ ]*\).*#'$me' \([^ #]*\).*/\1 \2/' > $tmp.1

wc $tmp.1
>$tmp.2
>$tmp.6

cat $tmp.1
cat $tmp.1 | while read md5 file
do
	cp $file $TMP
	chg=${file##*/pf-tfrag-}; chg=${chg%%-*}
	if badlines $file
	then
		echo "bad input $file"
		exit 1
	fi
	if mysort -u $file >> $tmp.x.$chg
	then
		echo $md5 $file >> $tmp.2
	fi
done
ls $tmp.x.* | sed "s+$tmp.x.++;"' /^[*]$/d' > $tmp.3
sed 5q $tmp.2
sed 5q $tmp.3
wc $tmp.?

# set up database for masterlogs
sed 's/.*/dump1 tapecat.&.gz/' < $tmp.3 | ng_rmreq -a -s $filer > $tmp.master
wc $tmp.master
sed 5q $tmp.master

err=0
for i in `cat $tmp.3`
do
	wfile=tapecat.$i
	if snag $wfile.gz		# sets inst
	then
		mysort -u -o $tmp.x.$i $tmp.x.$i
		gzip -d < $inst > $tmp.y.$i
		mysort -u -m $tmp.y.$i $tmp.x.$i | gzip > $tmp.$wfile.gz
		if [ `gzip -d < $tmp.$wfile.gz | md5sum` = `gzip -d < $inst | md5sum` ]
		then
			echo $inst > $tmp.$wfile.idem
			if badlines $tmp.xxx
			then
				echo "bad lines in $tmp.xxx"
				exit 1
			fi
		fi
	else
		echo "snag() failed"
		err=1
	fi
done

if [[ $err = 0 ]]
then
	for i in `cat $tmp.3`
	do
		wfile=tapecat.$i
		if [[ -f $tmp.$wfile.idem ]]
		then
			cat $tmp.$wfile.idem >> $tmp.6
		else
			if ng_spaceman_nm $wfile.gz > $tmp.8
			then
				dest=`cat $tmp.8`
				echo ${dest%\~} >> $tmp.6
				mv $tmp.$wfile.gz $dest
				ng_rm_register -H Steward $dest
				sleep 2
			else
				echo "tmp.3 spaceman failed"
				err=1
			fi
		fi
	done
fi

# verify
if [[ $err = 0 ]]
then
	sed 's/.* //' < $tmp.2 | xargs cat | mysort -u > $tmp-in
	xargs gzip -d -c < $tmp.6 | mysort -u > $tmp-out
	comm -23 $tmp-in $tmp-out > $tmp-erk
	if [[ -s $tmp-erk ]]
	then
		echo screwup in $tmp-erk
	
		# Send email if something is out of whack
		if [[ $nomail = false ]]
		then
	        	sendit 1
		fi
		date > $errfile
		exit 1
	fi
	wc $tmp-*
	> /tmp/log_gone
fi

# if still no errors, then remove log entries
if [[ $err = 0 ]]
then
	finish $tmp
	if [[ -n "$NG_TP_TFRAG_CODA" ]]
	then
		eval "$NG_TP_TFRAG_CODA"
	fi
	cleanup 0
else
	echo errors still exist

	# Send email if something is out of whack
	if [[ $nomail = false ]]
	then
        	sendit 2
	fi

	date > $errfile
	exit 1
fi

cleanup 0

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_tp_tfrag_comb:Combine tape fragment files)

&space
&synop	ng_tp_tfrag_comb [-r] [-x] [-v]

&space
&desc

&subcat Alerts and Mail
	Under certain conditions, &this causes alert messages to be  written  to the 
	master log. These alert messages will cause eMail to be delivered to any user
	listed on the &lit(NG_TAPE_MAIL) pinkpages variable. The message will be a 
	single line of information with a reference to a detail file usually in the 
	&lit($TMP) directory.

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&space 
&term	-r : Restart and finish after an error.
&space
&term	-v : Be chatty.
&space
&term	-x : Run in batch mode
&space
&endterms


&space
&parms

&space
&exit

&space
&see

&space
&mods
&owner(Andrew Hume)
&lgterms
&term	26 Mar 2007 (sd) : Replace hard mail code with alert logging.
&endterms


&scd_end
------------------------------------------------------------------ */

