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

#ifndef _ng_args_h 
#define _ng_args_h 1



/* --------- plan 9-style argument processing --------------------------*/
/* similar to what is in basic.h, but it removes anything that might
	depend on ast things (such as open012())
*/

#ifndef NG_BUFFER
#	define NG_BUFFER 8192
#endif

extern char *argv0;
extern int verbose;
#define	SET(x)
#define	USED(x)
#define	ARGBEGIN	for((argv0? 0: (argv0=*argv)),argv++,argc--;\
				argv[0] && argv[0][0]=='-' && argv[0][1];\
				argc--, argv++) {\
					char *_args, *_argt, *_argi;\
					int _argc;\
					_args = &argv[0][1];\
					if(_args[0]=='-' && _args[1]==0){\
						argc--; argv++; break;\
					}\
				while(_argc = *_args++)\
					switch(_argc)

#define	ARGEND		SET(_argt);SET(_argi);USED(_argt);USED(_argi);USED(_argc);USED(_args);}USED(argv);USED(argc);

#define	ARGF()		(_argt=_args, _args="",\
				(*_argt? _argt: argv[1]? (argc--, *++argv): 0))
#define	ARGSF()		(_argt=_args, _args="",\
				(*_argt? _argt: 0))
#define	ARGC()		_argc
#define	IARG(v)		if(_argi = ARGF()) v = strtol(_argi, 0, 0); else goto usage
#define	OPT_INC(v)	if(_argi = ARGSF()) v = strtol(_argi, 0, 0); else v++
#define	I64ARG(v)	if(_argi = ARGF()) v = strtoll(_argi, 0, 0); else goto usage
#define	SARG(v)		if(_argi = ARGF()) v = _argi; else goto usage

#endif

