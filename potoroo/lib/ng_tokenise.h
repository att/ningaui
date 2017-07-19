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
  ----------------------------------------------------------------------------------
	things for the tokeniser - moved out of ng_lib.h to prevent need to include sfio.h 
  ----------------------------------------------------------------------------------
*/


extern char **ng_readtokens( Sfio_t *f, char sep, char esc, char **reuse, int *count );	/* ng_token.c routines */
extern	char **ng_tokenise( char *b, char sep, char esc, char **reuse, int *count );

