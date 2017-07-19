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
#include	"ningaui.h"
#include	"woomera.h"

static void
traverse_j(Node *n, int st, char *pre, FILE *fp)
{
	char buf[NG_BUFFER];
	int i;

	sprintf(buf, "%s\t", pre);
	if((n->type == N_job) && ((st == 0) || (st == n->status))){
		fprintf(fp, "%snode %s[%d]: status=%d\n", pre, n->name, n, n->status);
	}
	for(i = 0; i < n->nprereqs; i++)
		traverse_j(n->prereqs[i].node, st, buf, fp);
}

void
listr(FILE *out)
{
	traverse_j(node_r, J_running, "", out);
}

void
lists(FILE *out)
{
	traverse_j(node_r, J_not_ready, "", out);
}

void
list(FILE *out)
{
	traverse_j(node_r, 0, "", out);
}
