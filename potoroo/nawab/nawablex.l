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

%{
#include	<stdio.h>
#include	<string.h>
#include	<stdlib.h>
#include	<ctype.h>
#include	<string.h>

#include	<sfio.h>

#include	"ningaui.h"
#include	"ng_ext.h"
#include	"ng_lib.h"
#include	"nawab.h"
#include	"nawabgram.h"

extern int lineno;
%}
%%

after		return AFTER;
attempts	return ATTEMPTS;
comment		return COMMENT;
consumes	return RESOURCES;
contentsf	return CONTENTSF;
contentsp	return CONTENTSP;
IGNORE_SIZE!	return IGNORESZ;
ignore_size!	return IGNORESZ;
in		return IN;
keep		return KEEP;
nodes		return NODES;
file		return FILEW;
partitions	return PARTITIONS;
partnum		return PARTNUM;
programme	return PROG;
program		return PROG;
priority	return PRIORITY;
rep		return REP;
required_inputs return REQIN;
woomera		return WOOMERA;
\"\"[^\n]*\"\"	{ 
			/* super string ""stuff""  can contain unescaped "s */
			yytext[yyleng-2] = 0; yylval.sval = ng_strdup(yytext+2); return STRING; 
		}
\"[^"]*\"	{ yytext[yyleng-1] = 0; yylval.sval = ng_strdup(yytext+1); return STRING; }
cmd[ \t]+[^\n]*	{
		char *p;

		for(p = yytext; !isblank(*p); p++)
			;
		while(isblank(*p))
			p++;
		yylval.sval = ng_strdup(p);
		return CMD;
	}
\%[ \t]*[\{A-Za-z_][A-Za-z0-9_\}]*	{
		char *p;

		for(p = yytext+1; isblank(*p); p++)
			;
		yylval.sval = ng_strdup(p);
		return PNAME;
	}

\%\$[A-Za-z_][A-Za-z0-9_]*	{
		yylval.sval = ng_pp_get( yytext+2, NULL );
		return STRING;
	}

\{[^}]*\}	{ yylval.sval = ng_strdup(yytext); return GROUP; }
\.\.	return DOTDOT;
[0-9]*[hsn]	{ yylval.sval = ng_strdup(yytext); return LIMIT; }
[A-Za-z][A-Za-z0-9_]*	{ yylval.sval = ng_strdup(yytext); return NAME; }
[A-Za-z0-9_/\-.\%#@]+	{ yylval.sval = ng_strdup(yytext); return VALUE; }
[A-Za-z][A-Za-z0-9_=]*\,[^ \t\n]*	{ yylval.sval = ng_strdup(yytext); return LIST; }
\![A-Za-z][A-Za-z0-9_]*	{ yylval.sval = ng_strdup(yytext); return NOTNAME; }
\(	return LPAR;
\)	return RPAR;
\[	return LSQ;
\]	return RSQ;
\-\>	return RARROW;
\<\-	return LARROW;
\`	return BQUOTE;
:	return COLON;
\;	return SEMICOLON;
=	return EQUALS;
\<	return LESSTHAN;
\>	return GREATERTHAN;
\:\=	return	CREATEDBY;
\#.*	{; /* comment */ }
#\n	{ lineno++; /* comment */ }
[ \t]+	{; /* white space */ }
\n	{ lineno++; /* white space */ }
.	{ return yytext[0]; }
%%
