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

#ifndef _ng_basic_h
#define _ng_basic_h 1

#include "ng_types.h"				/* ng_int64 and the like -- generated during precompile so not a part of this */

typedef ng_int64		ng_timetype; 	/* needed change for 64 bit stuff */

/* record reading stuff */
#define RDW_SIZE    	   4          		/* number of bytes in rdw field */
#define	ng_REC_RETRDW	0x01			/* if set, return address of rdw */
#define	ng_REC_RDW4	0x02			/* if set, return address of record after 4 byte rdw */
#define	ng_REC_PAD	0x04			/* if set, non-even sized records are padded */
#define ng_REC_SLACK	0x08			/* if set, allow bad RDW's */

#define TWO32           4294967296.		/* 2^32 */
#define TWO15           32768			/* 2^15 */

#define	HOUR		(3600*10)		/* .1s in a hour */
#define	DAY		(24*HOUR)		/* .1s in a day */

#define	NG_BUFFER	8192			/* generous buffer size */

#define	NGDEBUG(fn,lev)	((verbose & ((fn)<<4)) && ((verbose&0xF) >= (lev)))
#define	DBG_TAGIO	0x001			/* tagio functions */
#define	DBG_TAG		0x002			/* tag functions */
#define	DBG_APP1	0x004			/* application specific functions */

#define	Ii(x)   sizeof(x), (x)			/* for use with %I*d construcs in sf[s]printf statements */
#define	Ij(x)	sizeof(x), &(x)


/* --------- plan 9-style argument processing --------------------------*/

extern char *argv0;
extern int verbose;

/* we have some cases where USED is defined in the local source and we want to avoid pesky redef problems */
#ifndef USED
#define	SET(x)
#define	USED(x)		x=x
#endif

#define	ARGBEGIN	ng_open012( );\
			for((argv0? 0: (argv0=*argv)),argv++,argc--;\
			    argv[0] && argv[0][0]=='-' && argv[0][1];\
			    argc--, argv++) {\
				char *_args, *_argt, *_argi, *_argip;\
				int _argc;\
				_args = &argv[0][1];\
 				if( argv[0] && strncmp( argv[0], "--man", 5 ) == 0 ) { show_man( ); exit( 1 ); }\
				if(_args[0]=='-' && _args[1]==0){\
					argc--; argv++; break;\
				}\
				while((_argc = *_args++))\
				switch(_argc)
#define	ARGEND		SET(_argt);SET(_argi);USED(_argip);USED(_argt);USED(_argi);USED(_argc);USED(_args);}USED(argv);USED(argc);
#define	ARGF()		(_argt=_args, _args="",\
				(*_argt? _argt: argv[1]? (argc--, *++argv): 0))
#define	ARGSF()		(_argt=_args, _args="",\
				(*_argt? _argt: 0))
#define	ARGC()		_argc
#define	IARG(v)		if((_argi = ARGF())) v = strtol(_argi, &_argip, 0); else goto usage
#define	OPT_INC(v)	if((_argi = ARGSF())) v = strtol(_argi, 0, 0); else v++
#define	I64ARG(v)	if((_argi = ARGF())) v = strtoll(_argi, &_argip, 0); else goto usage
#define	SARG(v)		if((_argi = ARGF())) v = _argi; else goto usage

#endif

/* 
  Modified:	12 Dec 2003 (sd) : To call ng_open012() to ensure stdin/out/err are open to at least /dev/null.
		31 Jul 2005 (sd) : to eliminate warnings from gcc.
*/
