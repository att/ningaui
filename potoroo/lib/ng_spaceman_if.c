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
 ---------------------------------------------------------------------------
 Mnemonic:	ng_spaceman_if
 Abstract:	Request a filename from the managed file arenas.
 Date: 		26 March 2002
 Author:	E. Scott Daniels
 ---------------------------------------------------------------------------
*/
/*#include	<ast_std.h> */
#include	<string.h>
#include	<time.h>
#include 	<inttypes.h>
#include	<syslog.h>
#include	<unistd.h>
#include 	<sfio.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<errno.h>
#include	<dirent.h>
#include	<ctype.h>

#if	defined (OS_LINUX) || defined (OS_SOLARIS) || defined( OS_CYGWIN )
#include "sys/statvfs.h"
#else
#include "sys/param.h"
#include "sys/mount.h"
#include "sys/stat.h"
#endif

#ifdef OS_IRIX
#include	<sys/types.h>
#include	<sys/statfs.h>
#endif

#define NEW_SPACEMAN
#include	<ningaui.h>
#include	<ng_basic.h>
#include	<ng_lib.h>
#include	<ng_tokenise.h>


#define	ARENA_DIR	"/site/arena"
#define	CRIT		(LOG_CRIT * -1)		/* causees a crit to be written to log by bleat in addition to stderr */
#define	ERROR		(LOG_ERR * -1)

static	char **fslist = NULL;			/* pointers to filesystem names in our realm */
static	int *fshldcount = NULL;			/* number of hlds in each file system */
static	int nfslist = 0;			/* number allocated */
static	int fsl_idx = 0;			/* next insert */
static	int initialised = 0;
static	ng_uint64	full_threshold = 0;	/* we will not assign a filesys with less than this k bytes */
static	void get_fslist( char *arenad );
static	char *ng_root = NULL;			/* pointer to the root of our existance */


/* ---------------------- private ------------------------------------- */

/* linux has a wimpy statfs and so it must be different */
#if	defined  (OS_LINUX) || defined (OS_SOLARIS) || defined( OS_CYGWIN )
#define STAT statvfs
#else
#define STAT statfs
#endif

#ifdef OS_IRIX
#define GET_STATS(a,b)	STAT(a,b,sizeof(struct STAT),0)
#define BAVAIL	f_bfree
#else
#define GET_STATS(a,b)	STAT(a,b)
#define BAVAIL	f_bavail
#endif

static void sm_init( )
{
	char	*tok = NULL;

	initialised = 1;
	ng_srand( );

	if( (tok = ng_env( "NG_RM_FS_FULL" )) != NULL )
		full_threshold = atoll( tok );
	else
	if( (tok = ng_env( "RM_FS_FULL" )) != NULL )			/* backwards compatable */
		full_threshold = atoll( tok );
	else
		full_threshold = 500;		/* 500M */

	ng_bleat( 2, "spaceman: full_threshold = %I*d", Ii(full_threshold) );

	if( !(ng_root = ng_env( "NG_ROOT" )) )
	{
		ng_bleat( 0, "spceman_if: PANIC! cannot determing NG_ROOT" );
		exit( 11 );
	}
}

/* 
	looks at filesystem that holds name (any filename in that fs) 
	and returns true (1) if the available space is greather 
	than the threshold.	Threshold is defined as a percentage
	(e.g. 2 == 2% must be free to be ok)
*/
static int space_ok( char *name, long threshold )
{
        struct STAT info;

        if( GET_STATS(name, &info) < 0 )
	{
		ng_bleat( 0, "smif/space_ok: cannot stat filesystem containing: %s; %s", name, strerror( errno ) );
		return( 0 );
        }

	return (100 * info.BAVAIL)/info.f_blocks > threshold;

}

/* return the amount of free space (in meg)  on the filesystem containing the file/directory in name */
static ng_int64 get_availm( char *name )
{
        struct STAT info;
	double scale;
	double avail;

        if( GET_STATS(name, &info) < 0 )
	{
		ng_bleat( 0, "smif/get_availm: cannot stat filesystem containing: %s; %s", name, strerror( errno ) );
		return 0 ;
        }

	scale = info.f_bsize;
	avail = info.BAVAIL;

#if	defined(OS_LINUX) || defined(OS_IRIX) || defined(OS_SOLARIS)
	if(info.f_frsize)
		scale = info.f_frsize;
#endif
	scale /= 1024.*1024;
	avail *= scale;
	return (ng_int64) avail;
}

/*
	add a filesystem to our list and count its high level dirs 
*/
static void fs_add( char *name )
{

	DIR 	*d;
	struct	dirent *ent;

	if( fsl_idx >= nfslist )		/* need more */
	{
		nfslist += 10;
		fslist = ng_realloc( fslist, sizeof( char * ) * nfslist, "spaceman: fslist alloc" );
		fshldcount = ng_realloc( fshldcount, sizeof( int ) * nfslist, "spaceman: hld list alloc" );
	}

	fslist[fsl_idx] = ng_strdup( name );
	fshldcount[fsl_idx] = 0;

	if( (d = opendir( name )) != NULL )
	{
		while( (ent = readdir( d )) != NULL )
			if( isdigit ( *ent->d_name ) )		/* assume directories of interest are numbers */
				fshldcount[fsl_idx]++;		/* count high level directories that are here */

		closedir( d );
	}
	else
		ng_bleat( CRIT, "sm fs_add: could not read directory: %s: %s", name, strerror( errno ) );

	fsl_idx++;
}

/* get a list of filesystems that are under our control. arenad is 
   assumed to have n files. the first line of each file is the pathname 
   name of a file system that we are managing.  All regular files are 
   examined unless their names begin with the dot character.
*/
static void get_fslist( char *arenad )
{

	struct dirent 	*ent;		/* directory entry returned */
	DIR	*d;			/* pointer to open directory */
	char 	*buf;
	Sfio_t	*f;
	char	pname[255];

	if( ! arenad )
	{
		ng_bleat( CRIT, "smif/get_fslist: arena directory path is null" );
	}
	fsl_idx = 0;

	if( (d = opendir( arenad )) != NULL )
	{
		while( (ent = readdir( d )) != NULL )
		{
			/*if( ent->d_type == DT_REG && *ent->d_name != '.' )  linux does not return type it seems */
			if( *ent->d_name != '.' )			  /* ignore insignificant file names */
			{
				sfsprintf( pname, sizeof( pname ), "%s/%s", arenad, ent->d_name );
				if( (f = ng_sfopen( NULL, pname, "r" )) != NULL ) 
				{
					buf = sfgetr( f, '\n', SF_STRING );
					fs_add( buf );					/* create fs mgt block */
					sfclose( f );
				}
				else
				{
					sfprintf( sfstderr, "smif/get_fslist: cannot open file: %s: %s\n", pname, strerror(errno) );
				}
			}
		}

		closedir( d );
	}
	else
	{
		ng_bleat( CRIT, "smif/get_fslist: cannot open directory: %s: %s\n", arenad , strerror( errno ) );
	}
}

/* 
	reset the list of filesystems and counts
*/
void ng_spaceman_reset( )
{
	int	i;
	char	buf[255];

	if( !initialised )
		sm_init( );

	if( fslist != NULL )
	{

		for( i = 0; i < fsl_idx; i++ )
		{
			ng_free( fslist[i] );
			fslist[i] = NULL;
			fshldcount[i] = 0;
		}
	
		fsl_idx = 0;
	}

	sfsprintf( buf, sizeof( buf ), "%s%s", ng_root, ARENA_DIR );
	get_fslist( buf ); 			/* reread the list */
}

static int get_sldmax( )
{
	char	*tok;
	int	i;

	if( (tok = ng_env( "RM_SLDMAX" )) == NULL )
	{
		ng_bleat( ERROR * -1, "spaceman_get_sldmax: cannot find cartulary variable: RM_SLDMAX" );
		return 0;
	}

	i = atoi( tok );
	ng_free( tok );
	return i;
}

/* --------------------- public -----------------------------------------*/
/*
	return a null seperated list of filesystems, list is terminated with a null name
	e.g. name0name0name00
*/
void ng_spaceman_fslist( char *buf, int nbuf )
{
	char wbuf[256];
	int i, k, n;

	if( !initialised )
		sm_init( );

	if( ! fslist )
	{
		sfsprintf( wbuf, sizeof( wbuf ), "%s%s", ng_root, ARENA_DIR );
		get_fslist( wbuf );
	}
	k = 0;
	for(i = 0; i < fsl_idx; i++){
		n = strlen(fslist[i])+1;
		if(k+n > nbuf)
			break;			/* be silent; it'll never happen and small hurt if it is */
		memcpy(buf+k, fslist[i], n);
		k += n;
	}
	if(k < nbuf)
		buf[k] = 0;
}

/* build a null terminated list of tripples: filesystem hld-count avail-k.  each field is separated by sep
   result placed back in user supplied buf 
	nbuf is the size of the user's buffer 
*/
void ng_spaceman_fstlist( char *buf, int nbuf, char sep )
{
	char 	wbuf[NG_BUFFER];
	char	smbuf[256];
	int	i;

	if( !initialised )
		sm_init( );

	wbuf[0] = 0;
	for( i = 0; i < fsl_idx; i++ )
	{
		sfsprintf( smbuf, sizeof( smbuf ), "%s%c%d%c%I*d%c", fslist[i], sep, fshldcount[i], sep, sizeof( ng_int64 ), get_availm( fslist[i] ),sep );

		strcat( wbuf, smbuf );
	}

	if( i )
		wbuf[strlen(wbuf)-1] = 0;		/* ditch last seperator */
	sfsprintf( buf, nbuf, "%s", wbuf );
}

/*
	return the number of highlevel directories in filesystem fsname 
	returns -1 if fsname not found
*/
int ng_spaceman_hldcount( char *fsname )
{
	static	ng_timetype refresh_time = 0;

	char buf[256];
	ng_timetype now;
	int i;

	if( !initialised )
		sm_init( );

	if( ! fslist )
	{
		sfsprintf( buf, sizeof( buf ), "%s%s", ng_root, ARENA_DIR );
		get_fslist( buf );
	}
	else
	{
		now = ng_now( );
		if( now > refresh_time )
		{
			ng_spaceman_reset( );
			refresh_time = now + 600;
		}
	}

	for( i = 0; i < fsl_idx; i++ )
	{
		if( fslist[i] && strcmp( fslist[i], fsname ) == 0 )	/* bingo */
			return fshldcount[i];
	}

	return -1;
}

/*
	return a random local file pathname inside of fsname.  If fsname is null, 
	then we will randomly generate one by calling spaceman_rand, which in turn
	calls this routine after its decided on an fs name so indirectly this 
	routine is recursive.
	More so now that we guarentee that we dont return a collision to an existing
	file. If we detect that the filename is a collision we recurse to try again.
*/
char *ng_spaceman_local( char *fsname, long hldcount, char *fname )
{
	static	ng_timetype	refresh_time = 0;	/* when we need to force a refresh */
	static	int	maxsld = 0;			/* max number of second level directories */
	static 	int 	depth = 0;			/* prevent loops if something breaks */

#ifdef OS_FREEBSD
	struct stat sb;				/* never used, but needed to check for existance of file */
#endif
	ng_timetype	now;			/* time of invocation */
	int	hld;
	int	sld;
	long	target = 0;			/* the random target directory 0-m */
	char	*tok;
	char	buf[2048];

	if( !initialised )
		sm_init( );

	if( ++depth > 20 )		/* a state of panic */
	{
		ng_bleat( 0, "smif/spaceman_local: in too deep fname=%s", fname );
		return "/bogus/recursion_depth_check_in_spaceman_local/foo";
	}

	now = ng_now( );

	if( !fsname )					/* need to pick a random fsname too? */
	{
		if( !fslist )
		{
			sfsprintf( buf, sizeof( buf ), "%s%s", ng_root, ARENA_DIR );
			get_fslist( buf );
		}
		else
		{
			if( now > refresh_time )
			{
				ng_spaceman_reset( );
				refresh_time = now + 600;
			}
		}

		tok = ng_spaceman_rand( NULL, fname );		/* rand will calc a fsname, then call us */
		depth--;	
		if( ! tok )
			ng_bleat( 0, "spaceman_local: internal error(1); tok from rand was null %d(depth)", depth );
		return tok;	
/*

		idx = lrand48() % fsl_idx;

		hldcount = fshldcount[idx];	
		fsname = fslist[idx];			
*/
	}

	depth--;				/* safe here provided calls to rand are above */

	if( hldcount < 1 )
	{
		ng_bleat( 0, "spaceman_local: unusual condition: hldcount passed in was < 1: %d", hldcount );
		hldcount = 1;
	}
	
	if( now > refresh_time )
	{
		if( (maxsld = get_sldmax( )) < 1 )		/* incase cartulary changed */
			return NULL;

		refresh_time = now + 3000;
	}

	target = lrand48() % (maxsld * hldcount);
	hld = target / maxsld;
	sld = target % maxsld;

	if( *fsname == '/' )
		sfsprintf( buf, sizeof( buf ), "%s/%02d/%02d", fsname, hld, sld );
	else
		sfsprintf( buf, sizeof( buf ), "%s/%s/%02d/%02d", ng_root, fsname, hld, sld );

	if( fname )
	{
		if( (tok = strrchr( fname, '/' )) != NULL )
			fname = tok+1;
		strcat( buf, "/" );
		strcat( buf, fname );

#ifdef OS_FREEBSD
		if(  stat( buf, &sb) >= 0 )				/* dont return an existing file name */
			return( ng_spaceman_local( fsname, hldcount, fname  ) );
#endif
	}

	return ng_strdup( buf );
}

/*	return randomly selected fs as well as directory names 
	list contains space separated tripples: fsname, hdcount, size 
*/
char *ng_spaceman_rand( char *list, char *fname )
{
	static	ng_timetype refresh_time = 0;

	ng_timetype now;
	char	buf[NG_BUFFER];
	static char	**tlist = NULL;		/* pointer to list of tokens; we can use it on each successive call */
	int 	tcount;				/* number of tokens in the list */
	long	target;

	if( !initialised )
		sm_init( );

	if( ! list || ! *list )
	{
		now = ng_now( );
		if( now > refresh_time )
		{
			ng_spaceman_reset( );
			refresh_time = now + 1200;
		}

		ng_spaceman_fstlist( buf, sizeof( buf ), ' ' );		/* get list of tripples */

		if( ! *buf )
		{
			ng_bleat( 0, "sm/rand: fslist was empty! (%d)", fsl_idx );
			return ng_spaceman_local( "internal_spman_err", 1, fname );	/* something bogus */
		}

		list = buf;
	}

	if( (tlist = ng_tokenise( list, ' ', '\\', tlist, &tcount )) != NULL )
	{
		ng_uint64 *cum, point;
		int i;
		int j;

		if( tcount < 3 || !strchr( tlist[0] ? tlist[0] : " ", '/' ) )	/* must have at least 3, and first must be a pathname */
		{
			ng_bleat( 0, "spaceman_rand: bad fslist not enough tokens (found %d), or first tok is not a path: %s", tcount, tlist[0] ? tlist[0] : "null-string" );
			return ng_spaceman_local( "space_bad_fslist", 1, fname );	/* something bogus */
		}

		cum = ng_malloc(tcount*sizeof(cum[0]), "cum array");
		for(i = 2; i < tcount; i += 3)
		{
			cum[i/3] = atoi(tlist[i]);		/* set weights */
			if( cum[i/3] < full_threshold  || atoi( tlist[i-1] ) < 1 )	/* above threshold or no hlds */
				cum[i/3] = 0;			/* prevent selection */
		}
		point = cum[0];
		for(i = 1; i < tcount/3; i++)
			point = cum[i] += cum[i-1];		/* weights are now cummulative */

		/* selection is now easy; pick a random point and then find which segment hold sthat point */
		if( ! point )						/* no filesys with space */
		{
			ng_free(cum);
			return ng_spaceman_local( "no_rmmgr_space", 1, fname );	/* something bogus */
		}

		point *= drand48();
		for(target = 0; target < tcount/3; target++)
			if(cum[target] >= point)
				break;

		if(target >= tcount/3 )
			target--;
		ng_free(cum);

		j = tlist[(target*3)+1] ? atoi( tlist[(target*3)+1] ) : 1;		/* get the hld count for the target fs */
		if( j < 1 )
		{
			ng_bleat( 0, "spaceman_local: hld calculated to < 1! %d(target) %s(fsname), %s(tlist)", target, tlist[target*3], tlist[(target*3)+1] );
			return ng_spaceman_local( "space_calc_error", 1, fname );	/* something bogus */
		}
		return ng_spaceman_local( tlist[target*3] ? tlist[target*3] : "space_invaid_fslist", j, fname );
	}

	ng_bleat( 0, "spaceman_rand: no tokens produced by tokeniser! (%s)", list );
	return ng_spaceman_local( "space_bad_fslist", 1, fname );				/* something bogus */
}

/* add a directory path to the front of base that is in the currently acceptable location 
   for creating  new files.  If the propylaeum is in vogue, then the path will be to the 
   propy, if not the path will be selected from one of the repmgr managed file spaces
*/
char *ng_spaceman_nm( char *base )
{
	const char	*propy;
	char	*n = NULL;
	int	i = 5;

	while( n == NULL && i-- )				/* seems we sometimes get a null name back */
		n = ng_spaceman_rand( NULL, base );		/* assign at random leaning toward most empty */
	return n;


#ifdef KEEP_PROPY
	if( (propy = ng_env_c( "NG_PROPYLAEUM" )) == NULL )
	{
		ng_bleat( -2, "cannot find NG_PROPYLAEUM in cartulary/env" );
		exit( 1 );
	}

	if( base )
	{
		sfsprintf( buf, sizeof( buf ), "%s/%s", propy, base );
		return ng_strdup( buf );
	}

	return ng_strdup( propy );
#endif
}

#ifdef SELF_TEST
main( int argc, char **argv )
{
	char buf[4096];
	char *list;
	int i;
	int threshold;
	char	*c;
	int ok = 0;
	int fail = 0;
	int n = 10000;
	int j = 0;

	if( argc > 1 && strcmp( argv[1], "stress" ) == 0 )
	{
		if( argc > 2 )
			n = atoi( argv[2] );
		fprintf( stderr, "stress test of _nm and _rand functions starts; %d iterations\n", n );
		for( i = 0; i < n; i++ )
		{
			if( (c = ng_spaceman_rand( NULL, "rand-null-list" )) == NULL )
				fail++;
			else
				ok++;
			if( i < 10 && (c = ng_spaceman_nm( "reg_name" )) == NULL )
				fail++;
			else
				ok++;
			if( i < 10 && (c = ng_spaceman_local( 0, 0, "scooter_pies" )) == NULL )
				fail++;
			else
				ok++;


			if( (i % 1000) == 999 )
			{
				j++;
				fprintf( stderr, "batch completed: ok=%d  fail=%d; pausing  %d sec\n", ok, fail, 5 * j );
				ok = fail = 0;
				sleep( 5 * j );
				fprintf( stderr, "testing...\n" );
			}
		}

		fprintf( stderr, "@ end: ok=%d  fail=%d\n", ok, fail );
		exit( 0 );
	}

	if( argc > 1 && strcmp( argv[1], "badfs" ) == 0 )		/* test a bad fslist */
	{
		char key[1024];
		char val[1044];
		int x = 0;

		strcpy( val, "totspace=12345 5 5" );
		strcpy( val, "/ningaui/fs00 6 12818" );
		strcpy( val, "/ningaui/fs00" );
/*
		x= sscanf("fslist= \ntotspace=15264 freespace=1280018\ninstances=below\n", " %[^=]=%[^ \n]", key, val);
		x= sscanf("fslist=/ningaui/fs00.6.12818 \ntotspace=15264 freespace=1280018\ninstances=below\n", " %[^=]=%[^ \n]", key, val);
*/
		printf( "x=%d val=(%s)\n", x, val );
		/*c = ng_spaceman_rand( argv[2], "bad-fs-list" );	*/
		c = ng_spaceman_rand( val, "bad-fs-list" );	
		fprintf( stderr, "after bad flist call, c=%s\n", c );
		
		exit( 0 );
	}

	printf( "getting from random fs now\n" );
	for( i = 0; i < 10; i++ )
	{
		if( i == 6 )
			ng_spaceman_reset( );

		/*if( (c = ng_spaceman_local( NULL, 0, "local-null" )) != NULL ) */
		if( (c = ng_spaceman_local( NULL, 0, "nawab.b074.cpt" )) != NULL )
			printf( "%2d: %s\n", i, c );
		else
		{
			printf( "local-null failed!\n" );
			exit( 1 );
		}
	}

	for( i = 0; i < 10; i++ )
	{
		if( (c = ng_spaceman_local( "/ningaui/fs99", 30, "local-fs99" )) != NULL )
			printf( "%2d: %s\n", i, c );
		else
		{
			printf( "local-fs26 failed!\n" );
			exit( 1 );
		}

	}
	for( i = 0; i < 10; i++ )
	{
		if( (c = ng_spaceman_rand( NULL, "rand-null-list" )) != NULL )
			printf( "%2d: %s\n", i, c );
		else
		{
			printf( "rand-null-list failed!\n" );
			exit( 1 );
		}
		if( i < 10 && (c = ng_spaceman_nm( "reg_name" )) != NULL )
			printf( "%2d: %s  (used to be in propy)\n", i, c );
	}

	sfprintf( sfstdout, "hldcount for /ningaui/fs00 is: %d  avail k is: %I*d\n", ng_spaceman_hldcount( "/ningaui/fs00" ), sizeof( ng_int64 ), get_availm( "/ningaui/fs00" ) );
	sfprintf( sfstdout, "hldcount for /ningaui/fs01 is: %d  avail k is: %I*d\n", ng_spaceman_hldcount( "/ningaui/fs01" ), sizeof( ng_int64 ), get_availm( "/ningaui/fs01" ) );
	sfprintf( sfstdout, "hldcount for /ningaui/fs02 is: %d  avail k is: %I*d\n", ng_spaceman_hldcount( "/ningaui/fs02" ), sizeof( ng_int64 ), get_availm( "/ningaui/fs02" ) );
	sfprintf( sfstdout, "hldcount for /ningaui/fs03 is: %d  avail k is: %I*d\n", ng_spaceman_hldcount( "/ningaui/fs03" ), sizeof( ng_int64 ), get_availm( "/ningaui/fs03" ) );
	sfprintf( sfstdout, "hldcount for /ningaui/fs04 is: %d  avail k is: %I*d\n", ng_spaceman_hldcount( "/ningaui/fs04" ), sizeof( ng_int64 ), get_availm( "/ningaui/fs04" ) );
	sfprintf( sfstdout, "hldcount for /ningaui/fs05 is: %d  avail k is: %I*d\n", ng_spaceman_hldcount( "/ningaui/fs05" ), sizeof( ng_int64 ), get_availm( "/ningaui/fs05" ) );
	sfprintf( sfstdout, "hldcount for /ningaui/fs06 is: %d  avail k is: %I*d\n", ng_spaceman_hldcount( "/ningaui/fs06" ), sizeof( ng_int64 ), get_availm( "/ningaui/fs06" ) );

	ng_spaceman_fstlist( buf, 4096, ' ' );
	sfprintf( stdout, "fst list=(%s)\n", buf );
	ng_spaceman_fstlist( buf, 4096, ':' );
	sfprintf( stdout, "fst list=(%s)\n", buf );

	/*list = "fs00 23 1234 fs01 13 1234 fs02 20 1234 fs03 19 1234 fs04 8 1234 fs05 29 1234 fs06 40 1234 fs07 2 1234";
*/
	list = "fs00 1 1000 fs01 2 2000 fs02 3 3000 fs03 4 4000 fs04 5 5000 fs05 6 6000 fs06 7 7000 fs07 8 8000";
	if( argv[1] )
	{
		printf( "getting from: %s\n", list );
		for( i = 0; i < atoi(argv[1]) * 10; i++ )
		{
			if( (c = ng_spaceman_rand( list, "random" )) != NULL )
				printf( "%2d: %s\n", i, c );
			else
			{
				printf( "failed!\n" );
				exit( 1 );
			}
		}
	}

	printf( "-------- sumfs mimic test -------------\n" );
	memset( buf, 4085, 0 );
	ng_spaceman_fslist( buf, 4095 );
  	for(i = 0; buf[i]; )			/* this logic mimics sumfs logic for using fslist */
	{
		printf( "fslist element @%d = %s\n", i, buf+i );
                while(buf[i++] != 0)
                                ;
        }
	printf( "---------------------------------------\n" );

	for( threshold = 0; threshold < 99 && space_ok( "/ningaui/fs01/00", threshold ); threshold += 3)
		printf( "space  ok fs00 at %d\n", threshold );

		printf( "space NOT ok for fs00 for threshold %d\n",threshold );
}
#endif


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_spaceman_if:Spaceman interface routines)

&space
&synop	char *ng_spaceman_nm( char *base )
&break	char *ng_spaceman_local( char *fsname, int hldcount, char *fname )
&break	char *ng_spaceman_rand( char *list, char *fname )
&break	void ng_spaceman_reset( )
&break	int ng_spaceman_hldcount( )
&break	int ng_spaceman_fslist( char *buf, int nbuf )

&space
&desc	
	These routines provide an application interface to &ital(spaceman.)
&space
	.dv this ^&ital(ng_spaceman_nm)
	.dv This ^&ital(Ng_spaceman_nm)
	&This returns the &ital(basename) converted to a pathname in the
	managed area. All programmes should us &this to obtain fully qualified
	pathnames for files that will be registered with &ital(replication manager).

&space
	.dv this ^&ital(ng_spaceman_local)
	.dv This ^&ital(Ng_spaceman_local)
	&This generates a random pathname within the &ital(fsname) filesystem
	and returns the fully quallified pathname using the &ital(fname) string 
	as the basename. The &ital(fsname) filesystem name is assumed 
	to be any valid directory pathname within the &lit(NG_ROOT) directory.
	&ital(Hldcount) is the number of high level directories that currently
	exist under &ital(fsname.)
	If &ital(fsname) does not begin with a lead slash character (/), then 
	&this will assume that &ital(fsname) is a directory immediately under
	&lit(NG_ROOT) and will add the &lit(NG_ROOT) path to the front of the 
	pathname created.
&space
	In most cases, programmes will &stress(not) use this routine, but will
	use the &lit(ng_spaceman_nm) funciton. 
&space
	If &ital(fsname) is NULL, then a random filesytem on the current 
	node is selected, and the &ital(hldcount) parameter is ignored (the 
	current hldcount for the selected filesystem is calculated. See the 
	description of &lit(ng_spaceman_rand) for more information.

&space
	If &ital(fname) is NULL, then a directory path name is returned (without
	the trailing slash).

&space
	.dv this ^&ital(ng_spaceman_rand)
	.dv This ^&ital(Ng_spaceman_rand)
	&This generates a randomly selected pathname based on the filesystems
	in &ital(list.) If &ital(fname) is not NULL, then it is added to 
	the pathname generated. &ital(List) is expected to be a series of 
	space separated tripples: a filesytem name, the number 
	of high level directories in that file system, and the amount of available 
	space on the filesystem. 
	If the pointer to list is NULL, then a list is generated using the 
	local file systems.
	The resulting path name will have the syntax:
&space
&litblkb
	     /filesystemname/hh/ss[/filename]
&litblke
&space
	Where:
&indent
&begterms
&term	hh : 	Is the high level directory name which will be between 00 and 
	the hld count supplied in the tripplet for &ital(filesystemname.)
&space
&term	ss : Is the secondary level directory name which will be between 00 and 
	&lit(RM_SLDMAX.)
&endterms
&uindent

	&space
	Random file names are generated from the pool of available filesystems
	with preference given to those filesystems with the most space. If a 
	filesystem reports less than &lit(NG_RM_FS_FULL) meg available, then that 
	filesystem will not be selected.  If all filesystems report less 
	space available than the amount defined by &lit(NG_RM_FS_FULL), then 
	an invalid path will be returned (an open on the path should at somepoint
	fail freeing the user from needing to check the return status from &this.

	&space
	The &lit(ng_spaceman_reset) function causes the filesystem information
	to be purged and reread. This allows a long running process to 
	cause this information to update if necessary. Long running processes
	which invoke &lit(ng_spaceman_rand) and &lit(ng_spaceman_local) will 
	be refreshed automatically.

	&space
	The &lit(ng_spaceman_fslist) function returns a list of
	the filesystems on the current system. Each name is followed
	by a NULL, and another NULL follows the list.
	

&space
&opts	The following parameters are expected:
&space
&parms
&begterms
&term	fsname : A pointer to a character string that contains the filesystem
	name portion of the arena. This is  typically the &lit(fs00) portion 
	of &lit(/ningaui/fs00). If the string begins with a leading slash 
	character, then &this will NOT attempt to add the ningaui root path
	to the file name. 
	If &ital(fsname) is NULL, then a randomly selected filesystem from 
	those managed on the local node is selected. The list of managed filesystems
	is expected to be in &lit(NG_ROOT/site/arena.)
&space
&term	hldcount : Is the number of highlevel directories that exist in 
	&ital(fsname). If &ital(fsname) is not provided, this parameter is ignored.
&space
&term	fname : Is the file basename that will be added to the pathname 
	that is created. If a NULL pointer is supplied, then only the 
	directory name is returned to the caller. 
&endterms

&space
&exit	&This returns the pointer to a string (user must free) that contains
	the fully qualified path name. If there were errors, then NULL is returned. 

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 	26 Mar 2002 (sd) : New stuff.
&term	06 May 2002 (sd) : To correctly define tlist in rand function (static).
&term	02 Jun 2003 (sd) : Changed to prevent returning a name in a filesystem that 
	has less than RM_FS_FULL meg left. Also modified ng_sm_rand to accept a null
	list (generatest its own based on  the local file system).
	Turned off the propylaeum.
&term	19 Jul 2003 (sd) : Several tweeks to the stats call stuff for IRIX port.
&term	31 Jul 2003 (sd) : Corrected issue in fslist() -- bad use of buffer for pathname.
&term	11 Oct 2003 (sd) : Changed to prevent spaceman from returning the name of a 
	file if the selected path caused a collision.
&term	07 Jun 2005 (sd) : Downgraded CRIT to ERROR.
&term	31 Jul 2005 (sd) : changed to avoid gcc warnings.
&term	23 Aug 2006 (sd) : Now looks for NG_RM_FS_FULL first so that the value can 
		be overridden on the command line with  NG_RM_FS_FULL=900 ng_command.
&term	30 Aug 2006 (sd) : Converted strdup() calls to ng_strdup().
&term	11 Oct 2006 (sd) : Added some protection against core dumps resulting from bad
		fslist strings being passed to spaceman_rand().
&term	16 Nov 2006 (sd) : Converted sfopen() to ng_sfopen().
&term	01 Mar 2007 (sd) : Corrected a memory leak in get_sldmax()
&endterms

&scd_end
------------------------------------------------------------------ */
