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

#include	<sys/types.h>
#include	<sys/time.h>
#include	<sys/resource.h>
#include	<stdio.h>
#include	<sfio.h>
#include	<stdlib.h>
#include	<string.h>		/* needed for linux */
#include	"ningaui.h"
#include	"woomera.h"

#define	T(z)	((z).tv_sec + (z).tv_usec/1e6)

void
prtimes(char *comment, FILE *fp)
{
	struct rusage r;
	char buf[NG_BUFFER];
	time_t t;

	if(getrusage(RUSAGE_CHILDREN, &r)  < 0){
		perror("getrusage");
		fprintf(fp, "%s no usage available\n");
		return;
	}
	time(&t);
#ifdef OS_SOLARIS
	if(ctime_r(&t, buf, sizeof( buf ) )== 0)	/* maddening */
#else
	if(ctime_r(&t, buf)== 0)
#endif
		strcpy(buf, "<no ctime>");
	else
		*strchr(buf, '\n') = 0;
	fprintf(fp, "%s %s usr=%.3fs sys=%.3fs\n", buf, comment, T(r.ru_utime), T(r.ru_stime));
}
