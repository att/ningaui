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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include	<ningaui.h>
#include	<seq.man>

double	min=1.0;
double	max=0.0;
double	incr=1.0;
int	ctoi = 0;
int	cons=0;
char	*format = NULL;
char	*picture = NULL;

extern double atof();
extern char *strchr();

void usage();
void buildfmt();
int verbose = 0;

int main( int argc, char **argv)
{
	register int i, j, k, n;
	char buf[BUFSIZ], ffmt[BUFSIZ];
	double x;

	ARGBEGIN
	{
		case 'f':	SARG( format ); break;
		case 'i':	OPT_INC( ctoi ); break;
		case 'p':	SARG( picture ); break;
		case 'w':	OPT_INC( cons ); break;
		case 'v':	OPT_INC( verbose ); break;
usage:
		default:
			usage( );
			exit( 1 );
			break;
	}
	ARGEND

	max=atof(argv[argc-1]);

	switch( argc )
	{
		case 1: 
			break;
	
		case 2:
			min = atof( argv[argc-2] );
			break;

		case 3:
			incr = atof( argv[argc-2] );
			min = atof( argv[argc-3] );
			break;

		default:
			usage();
			exit( 1 );
			break;
	}

	if(incr==0){
		fprintf(stderr, "seq: zero increment\n");
		exit(1);
	}

	buildfmt();

	/*fprintf( stderr, "format=%s", format );  */

	n=(max-min)/incr;
	for(i=0; i<=n; i++)
	{
		x=min+i*incr;
		if( ctoi )
			fprintf( stdout, format, (int) x );
		else
			fprintf( stdout, format, x );
	}

	return 0;
}

void usage(){
	fprintf(stderr, "usage: seq [-w] [-i] [-f Cformat] [-p [-][0]picture]  [first [incr]] last\n");
	exit(1);
}

void buildfmt()
{
	char 	*p;
	char	buf[1024];
	int	left = 0;
	int	lead0 = 0;
	int	width;

	if( picture )			/* picture specification rules */
	{
		if( *picture == '-' )	
		{
			left++;				/* left justify */
			picture++;
		}

		if( *picture == '0' )
			lead0++;

		width = strlen( picture );			
		if( (p = strchr( picture, '.' )) != NULL )
		{
			*p++ = 0;
			snprintf( buf, sizeof( buf ), "%%%s%s%d.%df\n", left ? "-" : "", lead0 ? "0" : "", width, strlen( p ) );
		}
		else
			snprintf( buf, sizeof( buf ), "%%%s%s%d.0f\n", left ? "-" : "", lead0 ? "0" : "", width );
			
		format = strdup( buf );
	}
	else
	{
		if( ! format )		/* no format string supplied, generate one ensuring a constant width if asked for */
		{
			if( cons ) 
			{
				sprintf(buf, "%.0f", min);
				width=strlen(buf);

				sprintf(buf, "%.0f", max);

				if(strlen(buf)>width)
					width=strlen(buf);
				snprintf( buf, sizeof( buf ), "%%%d.0f\n", width );
				format = strdup( buf );
			}
			else
				if( ctoi )
					format = strdup( "%d\n" );
				else
					format = strdup( "%.0f\n" );
		}
		else				/* ensure user supplied format string has newline */
		{
			if( (p = strchr( format, '\n' )) == NULL )
			{
				snprintf( buf, sizeof( buf ), "%s\n", format );		/* add newline */
				format = strdup( buf );
			}
		}
	}

	return;
}

#ifdef OLD_STUFF
void Xbuildfmt()
{
	static char fmt[10]="%.0f";
	register char *t;
	char buf[32];

	if(cons){
		sprintf(buf, format, min);
		width=strlen(buf);
		sprintf(buf, format, max);
		if(strlen(buf)>width)
			width=strlen(buf);
	}

	if(picture)
		while(picture[0]=='0' && picture[1]!='.'){
			width++;
			picture++;
		}
	if(picture==0)
		return;
	if(picture[0]=='-')
		picture++;
	t=strchr(picture, '.');
	if(t){
		t++;
		if(*t==0)
			strcat(fmt, ".");
		else
			sprintf(fmt, "%%.%df", strlen(t));
	}
	strcat(fmt, "\n");
	format=fmt;
}
#endif

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_seq:Generate a sequence of numbers)

&space
&synop	
	ng_seq [-i] -f Cformat-string [first [incr]] last
&break
	ng_seq [-i] -p [- | 0]picture  [first [incr]] last
&break
	ng_seq [-i] [-w] [first [incr]] last

&space
&desc	&This generates a sequence of numeric values, one per record, to the standard
	output device.  The format of the output can be controlled using either the 
	format (-f) or picture (-p) options.
	The constant width (-w) option causes the largest width needed to be used 
	when writing all values. When using the constant 
	width option right justification is assumed. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-f format : Supplies a format string that can be used in a C printf statement. This allows
	hex values or right justification to be supplied, along with constant values (e.g. a=%02x).
&space
&term	-i : Treat the sequence value as an integer. Internally, the value is maintained as a floating point number
	making this flag necessary if a format like &lit(%d)  or &lit(%x) is used in a &lit(-f) format string.
&space
&term	-p picture : A 'picture' clause (e.g. 99999.00) that defines how each output record should
	be formatted. A leading zero (e.g. 0999.99) indicates that the values should be padded with 
	leading zeros. A leading dash (e.g. -xxxx.xx) causes the values to be left justified.
	Any character other than a dash and period can be used to define the picture.
&space
&term	-w : Used without the -f option to hat &this ensure that a constant width is used when formatting 
	the output values.
&endterms


&space
&parms	One parameter is required which is the ending (max) value.  Two other parameters may be 
	supplied: the starting value and the incremental amount.  If the incremental amount is 
	supplied the starting value must also be supplied. 

&space
&exit
	An exit code that is not zero indicates an error. 

&examp
	The following are example invocations of &this:
&space
&litblkb
   ng_seq 1 5 50
   ng_seq -f %.2f  0 .25 1
   ng_seq -i -f %x 16
   ng_seq -p 9.99 0 .25 1
   ng_seq -w 1 10 100
&litblke

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	18 Dec 2006 (sd) : Rewrite to make it ningauised and to make -p work. Added man page support.
&term	18 Jan 2009 (sd) : Corrected man page to include description of -w option.
&endterms


&scd_end
------------------------------------------------------------------ */
