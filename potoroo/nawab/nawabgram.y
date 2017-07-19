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
	The yyparse() invocation causes a list of descriptor blocks to be created. Descriptor blocks 
	describe a variable/value assignment, or a programme. A job name statment causes a new programme
	descriptor to be added to the list. Each statement (consumes, priority etc.) after causes information 
	to be added to the most recent descriptor.

	Once the end of the programme is encountered, when yyparse() returns, the list of descriptor blocks
	is traversed and troups/jobs are generated.  This two pass implementation makes error reporting to 
	the user a bit tricky.  Errors encounterd during yyparse() are reported via the yyerror() function.
	During the descriptor traversal, errors are reported via sbleat(). In both cases the yyerrors variable
	is incremented to count total errors during both passes.  The programme is activated only if yyerrors
	is zero after both passes. 

   Modified:	13 Sep 2004 (sd) : THING = { stuff } is not supported by bison; = is not legal; removed! 
		27 Mar 2006 (sd) : statement order is no longer important; warning issued if assignments made
			before the programme statement. 
		28 Mar 2006 (sd) : Some MAJOR changes:
			%varnames are allowed in all iterators now; some changes to DOTDOT were necessary to support that. 
			statements (consumes, priority, etc) can now be placed in any order. When encountered their
				data is added to the most recently added descriptor.
		04 May 2006 (sd) : Fixed bug introduced when eliminiating shift/reduce conflicts.  The file ( `"cmd"` )
			was broken because of it. 
*/

%start		everything

%{
#include	<stdio.h>
#include	<sfio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<string.h>
#include	"ningaui.h"
#include	"ng_lib.h"
#include	"ng_ext.h"
#include	"nawab.h" 

#define	YYDEBUG	1

extern	int	yyparse(void);

#define	gettxt(a,b)	a b

int	lineno;
int	yyerrors;
char	*yyerrmsg = NULL;
Desc	*descs = NULL;
Desc	*mrad = NULL;			/* most recently added */
Iter	*lastiter;
char	*prog_name;
int	keep_time;			/* user supplied seconds to keep the programme */

#define CAPTURE_PNAME(a,b)	char buf[1024]; sfsprintf( buf, sizeof( buf ), "%%%s", b ); a=ng_strdup( buf ); /* capture pname in buf with lead % */
%}

/* a GROUP is: { token token token } (and is saved WITH the braces) */
/* x_LIMIT is number with h or s appended (e.g. 9h)  for setting consumes limits */

%token		EQUALS SEMICOLON COLON LSQ RSQ LARROW RARROW LPAR RPAR LESSTHAN GREATERTHAN BQUOTE DOTDOT
%token		AFTER COEVAL IN NODES PARTITIONS FILEW REP CONTENTSF CONTENTSP KEEP
%token		NAME STRING VALUE CMD PNAME NOTNAME GROUP LIMIT 
%token		PROG LIST PARTNUM RESOURCES ATTEMPTS PRIORITY CREATEDBY WOOMERA REQIN COMMENT IGNORESZ

%union{
	int	intval;
	char	*sval;
	Desc	*desc;
	Value	*value;
	Range	*range;
	Rlist	*rlist;
	Iter	*iter;
	Io	*io;
}

/*%type <intval>	number*/
%type <desc>	statement job assignment call 
%type <sval>	PNAME CMD NAME NOTNAME VALUE STRING GROUP LIST orderclause nodes PROG programme consumes rep BQUOTE LIMIT  woomera comment  
%type <value>	value attempts priority reqin 
%type <rlist>	rlist
%type <range>	range range1
%type <iter>	iterator range2 
%type <io>	inputf outputf var

%%

everything:	statement			 { descs = $1; }
	;


statement:	statement	assignment	 { $$ = appenddesc($$, $2); }
	|	statement	programme	 { 
			if( $$ )			/* desc list was not zero -- possible leading assignments that will be ignored */
			{
				sbleat( 0, "WARNING: statements before programme statement will be ignored!" );
 				purge_desc_list( $$ );
			}
			prog_name = $2; 
			$$ = 0; 
		} 
	|	statement	call		 { $$ = appenddesc($$, $2); }
	|	statement	job		 { $$ = appenddesc($$, $2); }
	|	statement	orderclause	{ $$ = add2mrad( $$, $2, DT_after ); }
	|	statement	range		{ $$ = add2mrad( $$, $2, DT_range ); }
	|	statement	value		{ $$ = add2mrad( $$, $2, DT_value ); }		/* comment string */
	|	statement	comment		{ $$ = add2mrad( $$, $2, DT_comment ); }	/* new preferred */
	|	statement	consumes	{ $$ = add2mrad( $$, $2, DT_consumes ); }
	|	statement	priority	{ $$ = add2mrad( $$, $2, DT_priority ); }
	|	statement	attempts	{ $$ = add2mrad( $$, $2, DT_attempts ); }
	|	statement	nodes		{ $$ = add2mrad( $$, $2, DT_nodes ); }
	|	statement	woomera		{ $$ = add2mrad( $$, $2, DT_woomera ); }
	|	statement	reqin		{ $$ = add2mrad( $$, $2, DT_reqin ); }
	|	statement	inputf		{ $$ = add2mrad( $$, $2, DT_input ); }
	|	statement	outputf		{ $$ = add2mrad( $$, $2, DT_output ); }
	|	statement	CMD		{ $$ = add2mrad( $$, $2, DT_cmd ); }
	|	err			 { $$ = 0; }
	|				 { $$ = 0; }
	;

err:	error

programme:	PROG NAME		 { $$ = strdup( $2 ); keep_time = 0; }
	|	PROG NAME KEEP VALUE 	{ $$ = strdup( $2 ); keep_time = atoi($4); }
	|	PROG NAME SEMICOLON	 { $$ = strdup( $2 ); keep_time = 0; }
	|	PROG NAME KEEP VALUE SEMICOLON { $$ = strdup( $2 ); keep_time = atoi($4); }
	|	PROG SEMICOLON		 { fprintf( stderr, "null programme name!\n" ); $$ = 0; keep_time = 0;  }
	;

/*assignment:	PNAME EQUALS value	 { $$ = newdesc(T_dassign, $1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0); $$->val = $3; }*/
assignment:	PNAME EQUALS value	 { $$ = empty_desc(T_dassign, $1 ); $$->val = $3; }
	;

value	:	NAME			 { $$ = newvalue(T_vname, $1); }
	|	VALUE			 { $$ = newvalue(T_vstring, $1); }
	|	PNAME			 { $$ = newvalue(T_vpname, $1); }
	|	STRING			 { $$ = newvalue(T_vstring, $1); }
	|	BQUOTE STRING BQUOTE 	 { $$ = newvalue(T_bquote, $2); }
	;


job	:	NAME COLON {
			char *s;
			char buf[NG_BUFFER];
			static int id = 1;

			if(strcmp($1, "job") == 0){
				sfsprintf(buf, sizeof buf, "j%d_%d", getpid(), id++);
				s = strdup( buf );
			} else
				s = $1;

			$$ = empty_desc( T_djob, s );		/* make a new descriptor adding job name only */
	}
	;

call	:	PNAME LPAR VALUE RPAR	 { $$ = newdesc(T_dcall, $1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0); $$->val = newvalue(T_vstring, $3); }
	;

/* this allows for either after job1,job2,job3 or after job1  but not "after job1 after job2 after job3" */
orderclause:	AFTER LIST			{ $$ = $2; }
	|	AFTER NAME			{ $$ = $2; }
	;

comment:	COMMENT STRING			{ $$ = $2; }
	|	COMMENT EQUALS STRING		{ $$ = $3; }

range	:	range1 RSQ			 { 
			$$ = $1; $$->iter = lastiter; 
			if( lastiter ) lastiter->ref++; 
		}
	|	range1 range2			 { $$ = $1; lastiter = $$->iter = $2; }
	;

range1	:	LSQ PNAME			 { $$ = newrange($2); }
	;

range2	:	IN  LESSTHAN  iterator  GREATERTHAN  RSQ	 { $$ = $3; }
	|							 { $$ = 0; }
	;

iterator:	NODES				 { $$ = newiter(T_inodes); }
	|	NODES LPAR STRING RPAR 		{ $$ = newiter( T_inodes ); $$->app = $3; }
	|	PARTITIONS LPAR NAME RPAR	 { $$ = newiter(T_ipartitions); $$->app = $3; }
	|	PARTNUM LPAR NAME RPAR		 { $$ = newiter(T_ipartnum); $$->app = $3; }
	|	PARTNUM LPAR PNAME RPAR		 { 
			$$ = newiter(T_ipartnum); 
			$$->app = ng_malloc( strlen( $3 ) +2, "pnum iterator" ); 
			sprintf( $$->app, "%%%s", $3 ); 
		}
	|	PARTITIONS LPAR PNAME RPAR	 { 
			$$ = newiter(T_ipartitions); 
			$$->app = ng_malloc( strlen( $3 ) +2, "partitions iterator" ); 
			sprintf( $$->app, "%%%s", $3 ); 
		}
	|	value DOTDOT value STRING	 {	 /* iteration with format string:  start .. end "%04d" */
			$$ = newiter(T_iint);
			if( $1->type == T_vstring || $1->type == T_vpname )
				$$->lostr = ng_strdup( $1->sval );
			else
			{
				yyerror( " value ..  is not numeric or variable name" );
				yyerrors++;
			}
			if( $3->type == T_vstring || $3->type == T_vpname )
				$$->histr = ng_strdup( $3->sval );
			else
			{
				yyerror( " .. value  is not numeric or variable name" );
				yyerrors++;
			}
			$$->fmt = strdup( $4 );
		}
	|	value DOTDOT value	 {		/* we allow   0 .. 1000  or 0 .. %varname or %vname .. %v2name */
			$$ = newiter(T_iint);
			if( $1->type == T_vstring || $1->type == T_vpname )
				$$->lostr = ng_strdup( $1->sval );
			else
			{
				yyerror( " value ..  is not numeric or variable name" );
				yyerrors++;
			}
			if( $3->type == T_vstring || $3->type == T_vpname )
				$$->histr = ng_strdup( $3->sval );
			else
			{
				yyerror( " .. value  is not numeric or variable name" );
				yyerrors++;
			}
		}
	|	BQUOTE STRING BQUOTE		 { $$ = newiter(T_ibquote); $$->app = $2; }
	|	CONTENTSF LPAR PNAME RPAR	 { 
			$$ = newiter( T_ifile ); 
			$$->app = ng_malloc( strlen( $3 ) +2, "contents pname iterator" ); 
			sprintf( $$->app, "%%%s", $3 ); 
		}
	|	CONTENTSF LPAR STRING RPAR	 { 
			$$ = newiter( T_ifile ); 
			$$->app = ng_strdup( $3 ); 
		}
	|	CONTENTSF LPAR VALUE RPAR	 { 
			$$ = newiter( T_ifile ); 
			$$->app = ng_strdup( $3 ); 
		}
	|	CONTENTSP LPAR STRING RPAR	 { 
			$$ = newiter( T_ipipe ); 
			$$->app = ng_strdup( $3 ); 
		}
	;

consumes :	RESOURCES NAME			 { $$ = $2; }
	|	RESOURCES STRING		 { $$ = $2; }
	|	RESOURCES LIST			 { $$ = $2; }
	|	RESOURCES PNAME		{ CAPTURE_PNAME( $$, $2 ); }	/* capture name with lead % -- need as string not as value */
	|	RESOURCES EQUALS NAME		 { $$ = $3; }
	|	RESOURCES EQUALS STRING		 { $$ = $3; }
	|	RESOURCES EQUALS LIST		 { $$ = $3; }
	|	RESOURCES EQUALS PNAME		{ CAPTURE_PNAME( $$, $3 ); }	/* capture name with lead % -- need as string not as value */
	|	RESOURCES NAME  EQUALS LIMIT  {
			$$ = ng_malloc( strlen($2) + strlen( $4 ) + 3, "consumes" );	/* name=value<end>  needs 2 extra chrs */
			sprintf( $$, "%s=%s", $2, $4 );
		}
	|	RESOURCES EQUALS NAME  EQUALS LIMIT  {
			$$ = ng_malloc( strlen($3) + strlen( $5 ) + 3, "consumes" );
			sprintf( $$, "%s=%s", $3, $5 );
		}
	;

attempts :	ATTEMPTS EQUALS value		 { $$ = $3; }
	|	ATTEMPTS value			 { $$ = $2; }
	;

priority :	PRIORITY EQUALS value		 { $$ =  $3; }
	|	PRIORITY value			 { $$ =  $2; }
	;

nodes	:	NODES NAME			 { $$ = $2; }
	|	NODES LIST			 { $$ = $2; }
	|	NODES STRING			 { $$ = $2; }
	|	NODES PNAME			{ CAPTURE_PNAME( $$, $2 ); }	/* capture name with lead % -- need as string not as value */
	|	NODES NOTNAME			 { $$ = $2; }
	|	NODES GROUP			 { $$ = $2; }
	|	NODES EQUALS NAME		 { $$ = $3; }
	|	NODES EQUALS LIST		 { $$ = $3; }
	|	NODES EQUALS NOTNAME		 { $$ = $3; }
	|	NODES EQUALS GROUP		 { $$ = $3; }
	|	NODES EQUALS STRING		{ $$ = $3; }
	|	NODES EQUALS PNAME		{ CAPTURE_PNAME( $$, $3 ); }	/* capture name with lead % -- need as string not as value */
/*
	|	NODES EQUALS PNAME	{
			char buf[1024];
			sfsprintf( buf, sizeof( buf ), "%%%s", $3 );
			$$ = ng_strdup( buf );
		}
*/
	;

woomera : 	WOOMERA EQUALS STRING		 { $$ = $3; }
	|	WOOMERA EQUALS NAME		 { $$ = $3; }
	|	WOOMERA EQUALS PNAME		{ 
			char buf[1024];
			sfsprintf( buf, sizeof( buf ), "%%%s", $3 );
			$$ = ng_strdup( buf );
		}
	| 	WOOMERA STRING			 { $$ = $2; }
	|	WOOMERA NAME			 { $$ = $2; }
	|	WOOMERA PNAME		{ 
			char buf[1024];
			sfsprintf( buf, sizeof( buf ), "%%%s", $2 );
			$$ = ng_strdup( buf );
		}
	;


reqin   :	REQIN EQUALS value		 { $$ =  $3; }
	|	REQIN value			 { $$ =  $2; }
	;


inputf	:	LARROW var		 { $$ = $2; /*$3->next = $1; */ }
	|	LARROW var IGNORESZ	{ $$ = $2; $$->flags |= FLIO_IGNORESZ; }
	;

outputf	:	RARROW var rep		 { $$ = $2; $2->rep = $3; /* $3->next = $1; $3->rep = $4; */  }
	;

rep	:	REP LPAR STRING RPAR		 { 
			if( strcmp( $3, "all" ) == 0 )
				$$ = strdup( "e" );			/* causes s_mvfiles to get =e which generates rm_register -e */
			else
			if( strcmp( $3, "leader" ) == 0 ) 
				$$ = strdup( ",Leader" );		/* replicate default number, include leader */
			else
				$$ = strdup( $3 ); 			/* assume user string is accurate:  [#],hood-name */
		}
	|					 { $$ = strdup( "d" ); }	/* defaults for rm_register -d */
	;


var	:	PNAME				 { $$ = newio($1, 0, NULL, 0 ); }
	|	PNAME EQUALS value		 { $$ = newio($1, $3, NULL, 0 ); }
	|	PNAME EQUALS value CREATEDBY STRING	 { $$ = newio($1, $3, $5, 0 ); }
	|	PNAME EQUALS value CREATEDBY PNAME	 { 
			$$ = newio($1, $3, NULL, 0  ); 
			$$->gar = ng_malloc( strlen( $5 ) + 2, "grammar: var" );
			sprintf( $$->gar, "%%%s", $5 );
		}
	|	PNAME EQUALS rlist value	 { $$ = newio($1, $4, NULL, 0 ); $$->rlist = $3; $4->type = T_rlist;}
	|	PNAME EQUALS FILEW LPAR rlist value RPAR	 { 
			$$ = newio($1, $6, NULL, 0 );
			if($5->r){
				$$->rlist = $5;
				$6->type = T_rlist;
			}
			$$->filecvt = 1;
		}
	|	PNAME EQUALS FILEW LPAR value RPAR	 {	/* %v = file ( '"cat foo"') */
			$$ = newio($1, $5, NULL, 0 );
			$$->filecvt = 1;
		}
	;

rlist	:	range				 { $$ = newrlist($1); }
	|	rlist range			 { Rlist *r; for(r = $1; r->next; r = r->next); r->next = newrlist($2); $$ = $1; }
	;

%%

yywrap()
{
	return(1);
}

yyerror(char *s)
{
	extern	char *yytext;

	char m[256];
	yyerrors++;
	sfsprintf( m, sizeof( m ), "grammar error:  %s on line %d near: %s", s, lineno, yytext );
	/*ng_bleat( 0, "%s", m ); */
	sbleat( 0, "%s", m );			/* to log and to the user if netif has set a session id */

	if( ! yyerrmsg )			/* save first to send back to the user */
	{
		yyerrmsg = strdup( m );
	}

}
