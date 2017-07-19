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
# little thing to help with testing - restarts all bad jobs

doit=1
verb="submitting:"

while [[ $1 = -* ]]
do
        case $1 in
                -n)     doit=0; verb="would submit:";;
		-p)	target=$2; shift;;
		*)	echo "usage: $0 [-n] [-p programme]"
			exit
			;;
        esac

        shift
done

ng_nreq explain all |awk -v target="${target:-all"} '
        /programme/	{
                split( $3, a, "(" ); pname = a[1];
		if( target != "all" && pname != target )
			pname = "";				# not this one buster
        }

        /troupe/	{
                split( $3, a, "(" ); tname = a[1];
        }

        /failure:/ {
                if( pname && tname )
                {
                        print pname, tname
                        tname = "";
                }
        }
'>/tmp/list.$$

if [[ ! -s /tmp/list.$$ ]]
then
        echo "nothing found to restart"
        exit
fi

while read x
do
        if [[ $doit -gt 0 ]]
        then
		echo "$verb $x bad"
		ng_nreq resub $x bad
        else
                echo "$verb $x"
        fi
done </tmp/list.$$

rm /tmp/list.$$

