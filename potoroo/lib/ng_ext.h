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
   ======================================================================================
	D E P R E C A T E D ! ! !

	I have finally moved everything from ext.h into ng_lib.h and this file is now
	deprecated.   Old code converted from gecko should be changed to remove the 
	inclusion of this file.  This file will be maintained as an empty file so as 
	not to have to find every bloody occurance of ext.h and trash it. 
   ======================================================================================
*/

/* I cannot believe that we have anything that included just ext.h, but on the off chance 
   that we do, suck in the right thing for them.
*/
#ifndef _ng_lib_h
#include <ng_lib.h>
#endif

