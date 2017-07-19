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
*  Mnemoic:	ng_var_exp
*  Abstract:	Replace variables (e.g. %name) in a buffer with the value that the name
*		maps to in the symbol table.  Assuming the percent sign is used as the derefence 
*		operator, the following are legal expansions:
*			%scooter
*			%{scooter}junk
*			junk%scooter
*			%scooter(p1:p2:p3:p4)
*
*		The last will substitute the paramters (p1-p4) into the variable value where it 
*		finds $1-$4 in any order or with any number of occurances. Thus if the variable 
*		scooter had been defined as "now is the time for $1 to come to the aid of their $2"
*		and the string "%scooter(aardvarks:fellow aardvarks)" the result would be:
*		"now is the time for aardvarks to come to the aid of their fellow aardvarks"
*  Parms:	st - pointer to symbol table
*		namespace - symboltable name space key (a magic key 43086) is used for things that
*			are put into the table by this routine (parameter expansion)
*		sbuf - source buffer 
*		vsym - variable indicator % $ etc
*		esym - escape symbol so that \% will cause a % to go into the expanded buf
*		psym - Parameter seperator for %name(p1:p2p3) expansions
*		flags:	0x01 - create a new buffer for expansion; user must free
*			0x02 - Do NOT do recursive expansion on variables
*  Returns:	Pointer to the expanded buffer
*  Date:	6 January 2001
*  Author: 	E. Scott Daniels
* ---------------------------------------------------------------------------------------
*/

#include	<unistd.h>
#include	<stdio.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<ctype.h>

#include	<sfio.h>

#include	<ningaui.h>
#include	<ng_lib.h>
#include	<ng_ext.h>		/* our VEX_ constants live here */

#define F_PARMS		0x01		/* internal flag - expand parms first */
#define F_BRACKETED	0x02		/* name was encased in {}. we must replace them if name not in tab */

#define MAX_NAME	63		/* max size of a name */
#define MAX_BUF		4096		/* max buffer size */
#define MAGIC		43086		/* magic number for putting our parms into the symbol table */

static	char vreplace_buffer[MAX_BUF];	/* static buffer whose address is returned if user asks for a const buf */

char *ng_var_exp( Symtab *st, int namespace, char *sbuf, char vsym, char esym, char psym, int flags )
{
	Syment	*se;			/* element returned by the symbol table */
	char	name[MAX_NAME+1];	/* spot to save a var name for look up */
	char	pname[MAX_NAME+1];	/* parameter name buffer */
	char	plist[MAX_BUF];		/* list of parameters that were passed with variable */
	char	*obuf = NULL;		/* pointer to buf to return */
	char	*optr = NULL;		/* pointer at next insert point in output */
	char	*vbuf;			/* pointer to variable value string */
	char 	*oend;			/* prevent buffer overruns */
	char	*ebuf;			/* full expansion of variable - ready for cp to obuf */
	char	*eptr;			/* pointer into ebuf */
	int	nidx;			/* index into name buffer */
	int	pidx;			/* parameter list index */
	int	p;			/* parameter number */
	int 	iflags = 0;		/* internal flags */
	int 	need;			/* number of ) needed to balance ( */
	static int depth = 0;		/* prevent a never ending recursion by aborting if we go too deep */


	if( ++depth > 25 )
	{
		ng_bleat( 0, "ng_var_exp: panic: too deep" );
		ng_bleat( 0, "ng_var_exp: sbuf=(%s)", sbuf );
		ng_bleat( 0, "ng_var_exp: flags=0x02x", flags );
		ng_bleat( 0, "ng_var_exp: vsym=%c", vsym );
		abort( );
	}

	if( flags & VEX_NEWBUF )
		obuf = (char *) ng_malloc( MAX_BUF-1 * sizeof( char ), "ng_var_exp, buffer" );
	else
		obuf = vreplace_buffer;

	optr = obuf;
	oend = obuf + (MAX_BUF-1);

	while( sbuf && *sbuf && (optr < oend) )	
	{
		if( *sbuf == vsym )
		{
			nidx = 0;
			sbuf++;
			if( *sbuf == '{' )
			{
				iflags |= F_BRACKETED;
				sbuf++;
				while( nidx < MAX_NAME && *sbuf && *sbuf != '}' )
					name[nidx++] = *sbuf++;
				sbuf++;					/* step over end } */
				name[nidx] = 0;
			}
			else
			{
				while( nidx < MAX_NAME && *sbuf && (isalnum( *sbuf ) || *sbuf == '_' ) )
					name[nidx++] = *sbuf++;
				name[nidx++] = 0;

				if( *sbuf == '('  )		/* capture parameter after %name( */
				{
					sbuf++;				/* skip lead ( */
					iflags |= F_PARMS;				/* must expand for parms too */
					pidx = 0;
					p = 1;
					need=1;					/* number of ) needed */
					while( pidx < MAX_BUF-1 && *sbuf && need )	/* while we need ) and have room */
					{
						sprintf( pname, "%d", p );
						symlook( st, pname, MAGIC, plist+pidx, SYM_COPYNM );		/* add parm */

						while( pidx < MAX_BUF-1 && *sbuf && need && *sbuf != psym )
						{
							if( *sbuf == esym )
								sbuf++;
							else
							if( *sbuf == '(' )
								need++;
							else
							if( *sbuf == ')' )
								need--;

							if( need )
								plist[pidx++] = *sbuf++;

						}

						plist[pidx++] = 0;
						sbuf++;
						p++;
					}
				}
			}

			if( (se = symlook( st, name, namespace, 0, 0 )) && (vbuf = (char *) se->value) )
			{
				if( iflags & F_PARMS )
					vbuf = ng_var_exp( st, MAGIC, vbuf, '$', esym, psym, VEX_NORECURSE | VEX_NEWBUF );

				if( flags & VEX_NORECURSE )		/* recursion is not allowed */
				{				
					eptr = vbuf;
					ebuf = NULL;		/* free will not barf */
				}
				else				/* recurse to expand anything thats left */
					eptr = ebuf = ng_var_exp( st, namespace, vbuf, vsym, esym, psym, VEX_NEWBUF );

				while( *eptr && (optr < oend) )		/* add expanded stuff to output */
					*optr++ = *eptr++;	

				if( iflags & F_PARMS )		/* there were parameters - free parm expansion buffer */
					free( vbuf );
				free( ebuf );
			}
			else					/* name not in symtab */
			{
				*optr++ = vsym;
				if( iflags & F_BRACKETED )
					*optr++ = '{';			/* must replace these too */
				eptr = name;				/* just copy the name in  with lead var sym */
				while( *eptr && (optr < oend) )		/* add expanded stuff to output */
					*optr++ = *eptr++;	
				if( iflags & F_BRACKETED )
					*optr++ = '}';			/* must replace these too */
			}
		}
		else
		if( *sbuf == esym )
		{
			sbuf++;			/* skip it and add the next character as is */
			*optr++ = *sbuf++;
		}
		else
			*optr++ = *sbuf++;	/* nothing special, just add the character */
	}

	*optr = 0;
	depth--;
	return obuf;
}

#ifdef SELF_TEST

void show( Syment *se, void *data )
{
	printf( "show: @#%x space=%d (%s) == (%s)\n", se, se->space, se->name, se->value );
}
int main( int argc, char **argv )
{
	Symtab *st;
	Syment *se;
	char *e;
	char	pval[10];
	char	nval[10];
	int i;
	char *x = 0;

	*nval = *pval = 0;
	st = syminit( 17 );
	se = symlook( st, "fred", 0, "fred was here and he does not like %barney!", SYM_COPYNM );
fprintf( stderr, "here am i se=%x\n", se);
fprintf( stderr, "got #%x name=%s value=%s space=%d\n", se, se->name, se->value, se->space );
	symlook( st, "wilma", 0, "wilma is cool %fred", SYM_COPYNM );
	symlook( st, "barney", 0, "barney is a dork", SYM_COPYNM );
	symlook( st, "betty", 0, "betty is married to a dork", SYM_COPYNM );
	symlook( st, "bambam", 0, "bambam is loud and has betty for a mom and a dork for a dad", SYM_COPYNM );
	symlook( st, "dino", 0, "p4=$4 p3=$3 p1=$1 p2=$2 p4=$4", 0 ); 
	symlook( st, "p", 0, pval, SYM_COPYNM );
	symlook( st, "n", 0, nval, SYM_COPYNM );

	symtraverse( st, 0, show, NULL );
	if( argc < 2 )
	{
		fprintf( stderr, "usage: %s string-to-parse\n", argv[0] );
		exit( 1 );
	}
	
	fprintf( stderr, "argv[1] = (%s) \n", argv[1] );
	e = ng_var_exp( st, 0, argv[1], '%', '\\', ':', 0 );
	printf( "(%s) --> (%s)\n", argv[1], e );

	symtraverse( st, 0, show, NULL );

#ifdef KEEP
	for( i = 0; i < 20; i++ )
	{
		sprintf( pval, "%04d", i );
		sprintf( nval, "%04d", 20-i );
		e = ng_var_exp( st, 0, "junk-%p-stuff-%n", '%', '\\',  ':', 0 );
		printf( "(%s)\n", e );
	}
#endif
}
#endif



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_var_exp:Variable Expander)

&space
&synop	char *ng_var_exp( Symtab *st, int namespace, char *sbuf, char vsym, char esym, char psep, int flags )

&space
&desc	&This parses the &ital(sbuf) source buffer and expands any variables using the 
value that the variable name maps to in the symbol table &ital(st).
The a pointer to the resulting expanded string is returned.
A variable name is recognised in the source buffer as any string that begins with the &ital(vsym) character
continuing until a non-alphanumeric or underscore (_) character is encountered. 
Using the curly brace characters ({ and }), a variable name may be concatinated to text in the same 
mannar as is accomplished in the Korn shell (e.g. &lit(^&{foo}bar) would expand the variable foo and concatinate the 
characters &lit(bar) to the end).
The variable symbol (&ital(vsym)) is ignored if it is immediately preceded with the escape character (&ital(esym)).
If a variable name is not defined in the symbol table, then the unexpanded name (with the variable symbol) is 
placed into the expanded buffer.

.sp
If a variable's value contains references to other variables, they will be expanded before this routine returns 
control to the caller unless the caller has set the &lit(VEX_NORECURSE) flag. 
A variable value may also contain parameter references in the form of &lit($)&ital(n). These 
parameter references are replaced with parameter values that are contained within parenthesis if opening (left) parenthesis
is concatinated with the variable name (e.g. ^&ital(word)). &This determines the number of parameters "passed" using the 
&ital(psym) (parameter seperator) character passed to &this.

.sp
The following illustrates several examples of variable expansion assuming that the variable symbol passed to &this is 
the percent (%) character, the escape symbol is a carret (^^) character, and that the parameter seperator is a colon (:).
.sp
.ta b 2i 2i 2i 2i
VARIABLE NAME .cl : VALUE .cl : REFERENCE .cl : EXPANSION
.tr
ng_root .cl : /home/ningaui .cl :  cd %ng_root .cl :  cd /home/ningaui
.tr
admin .cl : adm .cl : cd %ng_root/%admin .cl : cd /home/ningaui/adm
.tr
logf .br id .cl : %ng_root/%adm/log .br 1234 .cl : %{logf}_%id .cl : /home/ningaui/adm/log_1234
.tr
cmd .cl : doit $1 $2 2>$1.err .cl : %cmd(filex:filey) .cl : doit filex filey 2>filex.log
.et



&space
&parms	The following parameters are expected to be passed into &this:
&begterms
&term	st : A pointer to the symbol table (as returned by the &lit(sym_init) library call).
&space
&term	namespace : The symboltable name space that is to be used when looking up the variable names.
&space
&term	sbuf : A pointer to a null terminated buffer containing the character string to parse.
&space
&term	vsym : The character that is to be recognised as the variable reference symbol.
&space
&term	esym : The character that is to be recognised as the escape symbol.
&space
&term	psep : The character that will be used to separate parameters enclosed within parenthesis 
	following the variable name.
&term	flags : Any of the various flag constants OR'd together:
&indent
	&beglist
	&term	VEX_CONSTBUF	: Causes &this to expand the string into a static buffer. The user may modifiy 
	the contents of the buffer following the call, but the next call to &this is likely to destroy the buffer.
&space
	&term	VEX_NEWBUF 	: &This returns the expanded string in a new buffer. The user must free the buffer
	to prevent memory leaks.
&space	
	&term VEX_NORECURSE :	If this is set, &this will &stress(not) recurse to expand variable references that 
	are contained in the initially expanded variable value. 
	&endlist
&uindent
&endterms

&space
&exit	If the variable is located in the symbol table, then the return from &this is a pointer to the 
	buffer containing the expansion. If &this was unable to work correctly, a NULL pointer is returned. 
	If variables encountered in the source string are not found in the symbol table, they are replaced 
	with a single space. 

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	06 Jan 2002 (sd) : Original code. 
&term	02 Mar 2006 (sd) : Added additional debug output if we panic. 
&endterms

&scd_end
------------------------------------------------------------------ */
