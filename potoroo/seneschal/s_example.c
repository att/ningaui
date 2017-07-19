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
  ----------------------------------------------------------------------------
  Mnemonic:	
  Abstract: 	example code to build a simple senschal job (no input/output files)
  Date:		20 Feb 2002
  Author: 	E. Scott Daniels
  ----------------------------------------------------------------------------
*/

/*	build a simple seneschal job - no input or output files specifically defined
	(seneschal will not block on files, nor will it attempt to version/rename/register
	the job's output)

	seneschal jobs buffers are newline terminated asci-z strings so they can easily be 
	sent and picked out of a tcp stream.

	size is the job size. seneschal uses this to try to predict the 'cost' of the job. in most
	cases it should be the size of all input files. jobs are queued in decending priority order, and 
	in decending size order for jobs with the same priority.
*/
char	*seneschal_job( char *type, char *name, int att, int size, int pri, char *rsrcs, char *node, char *cmd )
{
	char	*job[NG_BUFFER]

	if( sfsprintf( job, sizeof( job ), "%s_%s:%d:%d:%d:%s:%s:::cmd %s\n" type, name, att, size, pri, rsrcs, node, cmd ) < NG_BUFFER - 1 )
		return ng_strdup( job );
	else
		return NULL;		/* resulting string was too large! */
}

