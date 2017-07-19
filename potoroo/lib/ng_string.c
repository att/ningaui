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

#include	<stdio.h>
#include	<sfio.h>
#include	<stdlib.h>
#include	<string.h>
#include	"ningaui.h"

#define MAX_STR_LEN     20

ng_int64
ng_string_to_int64(ng_byte *ptr, int n)
{
  char temp[MAX_STR_LEN];
 
  temp[n] = ' ';
  memcpy(temp, ptr, n);
  return (ng_int64)strtoull(temp, 0, 10);
}
 
int
ng_string_to_int(ng_byte *ptr, int n)
{
 char temp[MAX_STR_LEN];
 
  temp[n] = (char) NULL;
  memcpy(temp, ptr, n);
  return atoi(temp);
}

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_string.c:Specialised string manipulation for Ningaui)

&space
&synop	ng_int64 ng_string_to_int64( ng_byte *ptr, int len ) .br
	     int ng_string_to_int( ng_byte *ptr, int len )

&space
&desc	Both &ital(ng_string_to_int64) and &ital(ng_string_to_int) accept
	a pointer to a buffer (&ital(ptr)) and buffer length &ital(len).
	The contents of the buffer are  assumed to be an ASCII 
	number which is converted into an integer. 
	&ital(Ng_string_to_int64) converts the 
	contents of the buffer into a 64 bit integer (longlong) while 
	&ital(ng_string_to_int) converts the buffer contents into an
	&lit(int).  The contents of the buffer does &stress(not) have 
	to be a NULL terminated string, and the buffer is preserved.

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 28 Mar 2001 (sd) : Converted from Gecko, added SCD.
&endterms

&scd_end
-------------------------------------------------------------------*/
