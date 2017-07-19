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


ps -ef | awk 'BEGIN	{ target = '$1' }
	{
		x = $0; sub(".*:...", "", x); cmd[$2] = x
		parent[$2] = $3
	}
function par(x, s, indent, goo){
	if(parent[x]+0 > 0) indent = par(parent[x], " ")
	else indent = ""
	goo = indent
	if(s != " "){
		goo = indent
		gsub(".", s, goo)
	}
	print goo x " " cmd[x]
	return indent "    "
}
function sib(x, indent){
	for(i in parent) if(parent[i] == x){
		print indent i " " cmd[i]
		sib(i, indent "    ")
	}
}
END	{
		x = par(target, ">")
		sib(target, x)
	}'
