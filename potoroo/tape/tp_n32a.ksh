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

#
# for now, if "." or "/" will cause problems, let the user
# fix them with tr
#


export bits
export chars


main () {
    (
	echo 2o
	cat -
	echo f
    ) |
    dc |
    tr -d '\134\012' |
    awk '
    function bindec(i) {
	n = 0

	for (j = 1; j <= length(i); j++) {
	    n = (2 * n) + substr(i, j, 1)
	}

	return n
    } # bindec

    BEGIN {
	bits = '$bits'
	chars = "'$chars'"
	pad = "000000"
    }

    {
	n = length($0) % bits
	if (n != 0) {
	    i = bits - n
	    $0 = substr(pad, 1, i) $0
	}

	for (b = 1; b <= length($0); b += bits) {
	    printf("%c", substr(chars, bindec(substr($0, b, bits)) + 1, 1))
	}
    }

    END {
	printf("\n")
    }'
} # main


setup () {
    chars="0123456789"
    if expr "$1" : '.*64' > /dev/null ; then
	chars="./${chars}ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	bits=6
    else
	bits=5
    fi
    chars="${chars}abcdefghijklmnopqrstuvwxyz"
} # setup


##### #####   MAIN PROGRAM STARTS HERE   ##### #####


setup $0

if [[ $# -eq 1 ]] ; then
    echo $1 | main
else
    cat - | main
fi
