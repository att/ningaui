/*
* ======================================================================== v1.0/0a157 
*                                                         
*       This software is part of the AT&T Ningaui distribution 
*	
*       Copyright (c) 2001-2009 by AT&T Intellectual Property. All rights reserved
*	AT&T and the AT&T logo are trademarks of AT&T Intellectual Property.
*	
*       Ningaui software is licensed under the Common Public
*       License, Version 1.0  by AT&T Intellectual Property.
*                                                                      
*       A copy of the License is available at    
*       http://www.opensource.org/licenses/cpl1.0.txt             
*                                                                      
*       Information and Software Systems Research               
*       AT&T Labs 
*       Florham Park, NJ                            
*	
*	Send questions, comments via email to ningaui_support@research.att.com.
*	
*                                                                      
* ======================================================================== 0a229
*/

/*
This is a special work round to get these three functions into 
a second library.   These funcitons are compiled without ast/sfio
references to support repmgrbt (threaded).  They are placed into 
libng_noast-g.a during precompile.  We have to 
do it this way or nmake has fits and tries to use the same .o for 
each library (go figure!)
*/
#define SANS_AST 1
#include "ng_testport.c"
#include "ng_read.c"
#include "ng_rx.c"
#include "ng_serv.c"
