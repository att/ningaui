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


# deprecated -- use tpreq unload (or somesuch)



exit 1






#!/usr/common/ast/bin/ksh

########
######## HACK! Will be replaced by a "real" program (or some other
######## mechanism) when the rest of the tape stuff is finished
########
######## Adam
########

#
# Load the standard configuration file
#
CARTULARY=${NG_CARTULARY:-$NG_ROOT/cartulary}
. $CARTULARY

#
# Load the standard functions
#
STDFUN=${NG_STDFUN:-$NG_ROOT/lib/stdfun}
. $STDFUN

tmp=$(get_tmp_nm tp-eject)

ng_get_nattr > $tmp
ng_del_nattr -p -h $(ng_sysname) $1
ng_get_nattr >> $tmp

mt -f $4 offline

#ng_mailer -f "tp_eject@$(ng_sysname)" -T ningaui@research.att.com -s "Please eject tape $1 from $2 ($3)" $tmp

cleanup 0
