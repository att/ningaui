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
	Mnemonic:	ng_roll_log -- roll a file from f to f.1 f.2 f.3 ... f.n
	Abstract:	
	
	Date:		07 September 2005 (hbd haz)
	Author:	E. Scott Daniels
---------------------------------------------------------------------------------
*/
#include	<sys/stat.h>
#include	<unistd.h>

#include	<sfio.h>
#include	<ningaui.h>


int ng_roll_log( char *fname, int n, int compress )
{
	struct stat	fstat;		/* stats on a file */
	char 	buf[1024];
	char	f1[1024];
	char	*f2 = NULL;
	int 	i;
	int 	status = 0;

	if( stat( fname, &fstat ) != 0 )	/* dont roll things if base log is not there to push in */
		return 0;

	for( i = n; i > 0; i-- )
	{
		sfsprintf( f1, sizeof( f1 ), "%s.%d%s", fname, i,  compress ? ".gz" : "" );
		if( f2 )
		{
			if( stat( f1, &fstat ) == 0 )
			{
				ng_bleat( 2, "ng_roll_log: moving %s -> %s", f1, f2 );
				rename( f1, f2 );
			}

			ng_free( f2 );
		}
		f2 = ng_strdup( f1 );
	}

	sfsprintf( f1, sizeof( f1 ), "%s.1", fname );				/* name without .gz if compressing */
	status = rename( fname, f1 );		/* final move of fred to fred.1 */
	if( compress )
	{
		sfsprintf( buf, sizeof( buf ), "gzip -f %s &", f1 );
		ng_bleat( 2, "ng_roll_log: scheduling gzip of %s as an asynch process: %s", f1, buf  );
		system( buf );
	}

	if( f2 )
		ng_free( f2 );

	return status;
}

#ifdef SELF_TEST
main( int argc, char **argv )
{

	verbose  =2;
	ng_roll_log( argv[2],  atoi( argv[1] ), atoi(argv[3] ) );

}
#endif


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_roll_log:Roll a log file)

&space
&synop	ng_roll_log( char *filename, int copies, int compress_flag )

&space
&desc 	&This will 'roll' the file into a series of versions numbered from 1 to 
	&ital(copies.)  If the compress flag is non-zero, then the file is compressed
	after it is renamed  to  *.1.  The compression is started as an asynchronous 
	task and will not affect the length of time that is required for this 
	function to process. 

&space
	For example, if the file name /tmp/log.fred is passed in, with a copies value of 
	3, and a comression flag of one, the file &lit(/tmp/log.fred.2.gz) is coppied to 
	&lit(/tmp/log.fred.3.gz), &lit(fred.1.gz) is coppied to &lit(fred.2.gz) and 
	&lit(/tmp/log.fred) is coppied to &lit(/tmp/fred.1) and then compressed.  

&space
&parms	The function accepts three parameters:
&begterms
&term	filename : This is a pointer to a NULL terminated string containing the filename of 
	the log file to roll.
&space
&term	copies : This is a positive intetger which defines the number of copies (*.n) to 
	keep.
&space
&term	 compress_flag : Passing a non-zero value in this parameter causes the file to be compressed
	after being moved to the *.1  name. If zero is passed, the file is not compressed.  Compressed 
	files will have a .gz suffix appended following the copy number. 
&endterms

&space
&exit
	The result of the last move (existing file to *.1) is returned as the return code. 

&space
&see

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	07 Sep 2005 (sd) : Sunrise. (HBD HAZ)
&term	12 Apr 2006 (sd) : Fixed man page.
&endterms

&scd_end
------------------------------------------------------------------ */
