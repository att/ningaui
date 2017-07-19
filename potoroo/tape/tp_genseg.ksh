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

CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}     # get standard configuration file
. $CARTULARY 

STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}  # get standard functions
. $STDFUN

if test $# -ne 2
then
	echo "usage: $0 poolid filelist"
	exit 1
fi
volid=__VOLID__
segid=__SEGID__
x=`ng_dt -i`_$$
poolid=$1
pre=`ng_spaceman_nm pf-tapeseg-$poolid-$x`
flist=$2
dumpdate=`date -e`
sysname=`ng_sysname`

# a little anal, but first check all files are here
xargs stat < $flist > /dev/null
if [ $? -ne 0 ]
then
	echo files unstatable in $flist
	exit 1
fi

# cheat a little; absorb mkdirf and mkcpio into here
while read f
do
	echo `md5sum $f | ng_rm_size` $dumpdate $sysname $volid $segid | sed 's:/.*/::'
done < $flist > $pre.dir

pax -w -s ':/.*/::' -x cpio < $flist > $pre.cpio

for f in $pre.*
do
	ls -l $f
	ng_rm_register -H ning18 $f
done

exit 0
