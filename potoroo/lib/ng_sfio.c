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
*	Mnemonic:	ng_sfio.c
*	Abstract:	An interface to the sfio stuff to make detecting errors easier.
*			ng_sfopen() -- opens the named file and pushes our discipline. if mode 
*				string begins with a K, then the process is killed on the first 
*				reported error for the stream.
*			ng_sfdc_error() -- pushes our discipline onto the stream for an already
*				opened stream.  If the aoe parm is 1, then we kill the process
*				on the first detected error.
*	Date:		28 Oct 2006
*	Author:		E. Scott Daniels
* ---------------------------------------------------------------------------------
*/

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sfio.h>

#include <ningaui.h>
#include <ng_lib.h>
		
int ng_sf_dexh( Sfio_t *f, int type, void* v,  Sfdisc_t* disc );	/* our exception handler */

struct stream_info {
	Sfdisc_t d;		/* discipline info for sfio (we can pass this as the disc since it is first */
	char	*name;		/* file name (better log messages only) */
	int	error;		/* error detected flag */
	int	errnum;		/* errno of the first error we received */
	int	alerted;	/* number of alerts; we stop after 50 or so to prevent an all out flood unless verbose is 9 */
	int	abort_on_err;	/* if set, we abort immediately on the first error */
};


/* ------------------------- public --------------------------------------------------- */
/* create and push our discipline onto the stack 
*/
int ng_sfdc_error( Sfio_t *f, char *name, int aoe )
{
	struct stream_info *sp;

	if( ! f )
		return -1;

	sp = ng_malloc( sizeof( struct stream_info ), "ng_sfio:open:ilist" );
	memset( sp, 0, sizeof( struct stream_info ) );

	sp->name = name ? ng_strdup( name ) : strdup( "unnamed" );
	sp->abort_on_err = aoe;
	sp->d.exceptf = ng_sf_dexh;		/* set only exception; read/write/seek done by unerlying discipline */

	if( sfdisc( f, (Sfdisc_t *)sp ) == NULL )
	{
		ng_bleat( 0, "ng_sfdc_error: could not push discipline: %s", strerror( errno ) );
		return -1;
	}
	else
		ng_bleat( 4, "ng_sfdc_error: successfully pushed discipline onto the stack for %s", name );

	return 0;
}


Sfio_t *ng_sfopen( Sfio_t *of, char *fn, char *mode )
{
	Sfio_t *f = NULL;
	int	aoe = 0;		/* abort on error */

	if( *mode == 'K' )		/* user would like us to kill the process on the first error for this stream */
	{
		aoe++;
		mode++;
	}

	if( (f = sfopen( of, fn, mode )) == NULL )
		return NULL;


	if( ng_sfdc_error( f, fn, aoe ) < 0 )
	{
		sfclose( f );
		return NULL;
	}

	ng_bleat( 4, "ng_sfopen: stream opened: f=0x%x name=%s", f, fn );

	return f;
}

Sfio_t *ng_sfpopen( Sfio_t *of, char *cmd, char *mode )
{
	Sfio_t *f = NULL;
	int	aoe = 0;		/* abort on error */

	if( *mode == 'K' )		/* user would like us to kill the process on the first error for this stream */
	{
		aoe++;
		mode++;
	}

	if( (f = sfpopen( of, cmd, mode )) == NULL )
		return NULL;


	if( ng_sfdc_error( f, cmd, aoe ) < 0 )
	{
		sfclose( f );
		return NULL;
	}

	ng_bleat( 4, "ng_sfopen: pipe opened: f=0x%x cmd=%s", f, cmd );

	return f;
}


/* ---------------------------------- discipline ------------------------------------ */
/* the exception handler

	from sfio man page: 
		Unless noted otherwise, the return value of (*exceptf)() is used as follows:
			<0: The on-going operation shall terminate.
			>0: If the event was raised due to an I/O error,
				the error has been repaired and the on-going operation shall continue normally.
			=0: The on-going operation performs default actions with respect to the raised event.
				For example, on a reading error or reaching end of file, the top stream of a stack
				will be popped and closed and the on-going operation continue with the new top
*/
int ng_sf_dexh( Sfio_t *f, int type, void* v,  Sfdisc_t* disc )
{
	int i;
	struct stream_info *sp;
	int	bleat_level = 9;	

	sp = (struct stream_info *) disc;		/* disc struct we gave sfio is really our stream info with a disc at the front */

	if( !sp )
	{
		ng_bleat( 4, "ng_sfio:dexh: (%d); could not map f to stream_info", type );
		return 0;
	}

	bleat_level = sp->alerted++ > 50 ? 9 : 4;

	switch( type )
	{
		case SF_READ:		 		/* we assume error */
		case SF_WRITE:
			if( sfeof( f ) )		/* at end does not cause us to mark an error */
				ng_bleat( bleat_level, "ng_sfio:dexh:SF_READ/WRITE (%d) name=%s f=0x%x; at eof error=%d", type, sp->name, f, sp->error );
			else
			{
				if( errno != EINTR && errno != EAGAIN )		/* allow for default processing if either of these */
				{
					int *j = v;			/* keeps the compiler from yammering about deref of void ptr */

					if( sp->abort_on_err )
					{
						ng_bleat( 0, "ng_sfio:dexh: abort on error set: SF_READ/WRITE (%d): name=%s f=0x%x errno=%d oprc=%d", type, sp->name, f, errno, *j );
						sfdisc( f, NULL );			/* pop it -- prevent further calls */
						exit( 2 );
					}

					ng_bleat( bleat_level, "ng_sfio:dexh: SF_READ/WRITE (%d): name=%s f=0x%x errno=%d val=%d", type, sp->name, f, errno, *j );
					sp->error = 1;
					if( ! sp->errnum )
						sp->errnum = errno;		/* record the error number */
					return -1;				/* should prevent op from continuing */
				}
			}
			break;
			
 		case SF_NEW:  			/* called when closing and reopening by sfnew() */
		case SF_CLOSING:
			{
				int rc = 0;

				if( sp->error )
				{
					if( sp->errnum )			/* something captured */
					{
						errno = sp->errnum; 
						rc = - sp->errnum;
					}
					else
						errno = rc = -1;
				}
				else
					errno = 0;
				
				ng_bleat( bleat_level, "ng_sfio:dexh: CLOSING (%d): name=%s f=0x%x err=%d ret=%d", type, sp->name, f, sp->error, rc );
				
				sfdisc( f, NULL );			/* pop it */
	
				return rc;
			}
			break;

		case SF_DPOP:  			/* discipline is being popped -- trash it */
			ng_bleat( bleat_level, "ng_sfio:dexh: SF_DPOP (%d): name=%s f=0x%x", type, sp->name, f );
			free( sp->name );
			free( sp );
			break;

		default:
			break;
	}

	return 0;
}

/* ----------------------------------------------------------------------------------------------- */
#ifdef SELF_TEST
int verbose = 4;

main( int argc, char **argv )
{
	char	rbuf[NG_BUFFER];
	char *buf;
	Sfio_t *f;		/* input */
	Sfio_t *f2;		/* output */
	long count = 0;
	int x;
	int n;
	
	if( argc < 2 || strcmp( argv[1], "-?" ) == 0 )
	{
		ng_bleat( 0, "usage: %s file-to-read file-to-write", argv0 );
		exit( 1 );
	}

	if( strncmp( argv[1], "cmd ", 4 ) == 0 ) 
	{
		f = ng_sfpopen( NULL, argv[1]+4, "r" );
		if( f == NULL )
		{
			ng_bleat( 0, "popen failed! %s: %s", argv[1], strerror( errno ) );
			exit( 1 );
		}
	}
	else
	if( strcmp( argv[1], "stdin" ) != 0 )
	{
		f = ng_sfopen( NULL, argv[1], "r" );
		if( f == NULL )
		{
			ng_bleat( 0, "open failed! %s: %s", argv[1], strerror( errno ) );
			exit( 1 );
		}
	}
	else
	{
		f = sfstdin;
		if( ng_sfdc_error( f, "stdin", 1 ) < 0 )
		{
			ng_bleat( 0, "unable to push discipline on stdin" );
			exit( 1 );
		}
	}

	if( strcmp( argv[2], "stdout" ) != 0 )
	{
		f2 = ng_sfopen( NULL, argv[2], "Kw" );
		if( f2 == NULL )
		{
			ng_bleat( 0, "open failed! %s: %s", argv[1], strerror( errno ) );
			exit( 1 );
		}
	}
	else
	{
		f2 = sfstdout;
		if( ng_sfdc_error( f2, "stdout", 1 ) < 0 )
		{
			ng_bleat( 0, "unable to push discipline on stdout" );
			exit( 1 );
		}
	}


	while( (n = sfread( f, rbuf, sizeof( rbuf ) )) > 0 )
	{
		count++;
		sfwrite( f2, rbuf, n );
	}

	ng_bleat( 0, "closing input file; count=%ld",count );
	x = sfclose( f );
	ng_bleat( 0, "ng_sf_close returned %d [%s]", x, x == 0 ? "OK" : "an error" );

	ng_bleat( 0, "closing output file" );
	x = sfclose( f2 );
	ng_bleat( 0, "ng_sf_close returned %d [%s]", x, x == 0  ?"OK" : "an error" );

	exit( 0 );
}
#endif


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_sfio:Ningaui interface to sfio)

&space
&synop	Sfio_t *ng_sfopen( Sfio_t *f, char *name, char *mode )
&break 	int ng_sfdc_error( Sfio_t *f, char *name, int aoe )

&space
&desc	These routines provide an interface to the sfio environment that allows for simplified
	error checking on a stream opened for reading and/or writing.  The &ital(ng_sfopen)
	function will open the named file, reusing &ital(f) if it is not NULL, and will push
	the ningaui error discipline onto the stream.  If the first character of the mode 
	string is 'K', the process will be exited (killed) with an exit(2) when the first error
	is detected on the stream.  The 'K' is removed from the mode string before invoking
	any sfio functions to actually open the file. 
&space
	For streams that are already opened, the &ital(ng_sfdc_error()) function can be used 
	to push the ningaui error discipline onto the stream (f must not be null).  
	The name is purely to make debugging (ng_bleat) messages better and can be set to NULL.  
	The &ital(aoe) flag, when set to 1, has the same kill effect as described for &ital(ng_sfopen.)
&space
	Regardless of which function is used, the effect of the discipline is to track errors on 
	the stream and if the process is not aborted, to cause the sfclose() function to return 
	a non-zero value if an error was ever noticed on the stream.  (We also assume that 
	a non-zero value will be returned by sfclose() if there are issues with the close process
	itself even if there were no previously detected errors on the stream.

&exit
&subcat ng_sfopen
	Returns a pointer to the sfio datastructure used to manage the open stream. If the named file
	cannot be opened, NULL is returned.  The global variable &lit(errno) is left set and should 
	inidcate the reason of failure. The pointer returned is the same pointer that would have been 
	returned if the &lit(sfopen()) routine had been invoked directly.
&spcae
&subcat ng_sfdc_error
	The return value from this function is 0 to indicate success and <0 to indicate failure. 


&space
&see	sfio(3)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	30 Oct 2006 (sd) : Its beginning of time. 
&term	02 Jan 2007 (sd) : Corrected issue with warning generated under SuSE.


&scd_end
------------------------------------------------------------------ */
