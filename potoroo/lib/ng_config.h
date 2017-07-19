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

#ifndef _ng_config_h
#define _ng_config_h 1

/* these cause small mem leaks, but they are a quick fix to not using NG_ROOT at compile time */

#define DS_PATH		"/ningaui" 

/* default log info */
#define LOG_NAME	"/site/log/master"
#define LOG_DIR		"/site/log" 

/* tap database */
#define	TAP_DB		"/adm/tap.db" 

/* woomera's nest */
#define	WOOMERA_NEST	"/site/woomera" 

#define	DSNFILE		 "/adm/file_catalogue" 
#define	DSNPATH		 "/site/catalogue" 		/* where file catalogues (file num -->name map live */

#define	TMP		 "/tmp" 

#define	ADM		 "/adm" 


#endif
