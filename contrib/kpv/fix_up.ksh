#
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


# fix stuff in kpv source so that we can build it

function mod_makefile
{
	if ! grep GLMK_CC $1 >/dev/null 
	then
		cp $1 $1-
		sed 's/VCSFIO=0/VCSFIO=1/; s/; exit 0/|| true/g; s/CC=cc/CC = $(GLMK_CC)/; s/^CXFLAGS=/CXFLAGS = -D_USE_LARGEFILES -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE=1 -D_LARGEFILE_SOURCE=1 /' <$1- >$1
	else
		echo "already modified: $1"
	fi
}

# add an include of our fingerprint file
function add_fingerprint
{
	if ! grep sm_fingerprint.h $1 >/dev/null
	then
		echo "adding fingerprint: $1"
		cp $1 $1-
		echo "#include <sm_fingerprint.h>		/* ningaui fingerprint */" >$1
		cat $1- >>$1
	else
		echo "already fingerprinted: $1"
	fi
}

(
	find . -name Makefile 
	find . -name makefile
)| while read x
do
	echo adjusting $x
	mod_makefile $x
done


for x in "$@"
do
	find . -name $x | while read y 
	do
		add_fingerprint $y
	done
done
