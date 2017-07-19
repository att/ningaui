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
# Mnemonic:	spacehogdiffs
# Abstract:	
#		
# Date:		
# Author:	Mark Plotnick
# ---------------------------------------------------------------------------------
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN



# -------------------------------------------------------------------
ustring="[-d daysback] dir"
daysback=1
while [[ $1 = -* ]]
do
	case $1 in 
		-d)	daysback=$2; shift;;
		--man) show_man; exit 100;;
		*)	usage; exit 100;;
	esac

	shift
done

if test $# -gt 0
then
	dir=$1
else
	usage;
	exit 100
fi

# search up directory tree, looking for a spacehogs.0 file
if ! cd $dir
then
	echo $dir does not exist or is not accessible
	exit 1
fi
until test -f spacehogs.0 -o $(pwd -P) = "/"
do
	cd $(pwd -P)/..
done
if test $(pwd -P) != $dir
then
	echo using $(pwd -P) as base directory
fi
older=spacehogs.$daysback
newer=spacehogs.0
if ! test -f $newer
then
	echo $newer does not exist
	exit 1
fi
if ! test -f $older
then
	echo $older does not exist
	exit 1
fi
## doesn't work - ESPIPE join -1 2 -2 2 -a 2 <(sort +1 $older) <(sort +1 $newer)
sort +1 $older > /tmp/hogs_older.$$
sort +1 $newer > /tmp/hogs_newer.$$
join -1 2 -2 2 -a 2 /tmp/hogs_older.$$ /tmp/hogs_newer.$$ | awk '{
# NF == 2: directory only shows up in the newer file
	if (NF == 2) printf("%-23s\t-\t%d\t%+d\t+100%%\n", $1, $2, $2);
	if (NF == 3 && $2 != $3) printf("%-23s\t%d\t%d\t%+d\t%+d%%\n", $1, $2, $3, ($3-$2), 100*($3-$2)/$2);
}' | sort -n -r +3
cleanup 0


cleanup 0



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_spacehogsdiffs:Show differences between output produced by ng_spacehogs)

&space
&synop	ng_spacehogsdiffs [-d daysback] dir

&space
&desc	&This will do something. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&endterms


&space
&parms

&space
&exit

&space
&see

&space
&mods
&owner(Mark Plotnick)
&lgterms
&term	01 Oct 2007 (sd) : Added skeleton man page.
&endterms


&scd_end
------------------------------------------------------------------ */

