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
* -----------------------------------------------------------------------------
* Mnemonic:     spaceman
* Abstract:     This is the replication manger's file system space/directory manager.
*		It reads a dump from the instance database (rm_db -p) and tallies the 
*		number of files in each of the various tiers in each file system.
*		we assume for a filesystem /ningaui/fsxx/hh/ll:
*			there are 0-ll directories allocated when hh is created
*			second level directories (ll) will contain no more than max_sld files
*			
*		from the assumptions we can calculate the 'max' number of files that should
*		be placed in the existing directories. If this max exceeds a threhold, then 
*		we will add directories at the hh level.
* Date:         06 Jun 2001 - converted from original spaceman (daemon) 11 Apr 2002
* Author:       E. Scott Daniels
* assumptions:  
*       rm_db -p will dump the database of replication manager files
*	filesystem names will be of the form: /root/name
* ---------------------------------------------------------------------------
*/
#define 	x_AST_STD_H 1		/* fake it out or ast barfs when dirent.h included */

#include	<sys/stat.h>
#include	<sys/types.h>
#include	<stdio.h> 
#include	<unistd.h>
#include	<stdlib.h>
#include	<syslog.h>
#include	<stddef.h>
#include	<errno.h>
#include	<fcntl.h>
#include	<string.h>

#include	<dirent.h> 	/* causes wierd things unless _AST_STD_H is declared */

#include	<sfio.h>

#include	<ningaui.h> 
#include	<ng_basic.h>
#include	<ng_lib.h>
#include	<ng_ext.h>		/* symbol table extents */

#include 	"spaceman.man"

/* -------------------------------------------------------------------------------------------------- */

#define	MAX_FS		1024		/* max number of file systems we will manage */
#define MAX_HLD		1024		/* max num of high level directories we will manage */

#define ERROR		(LOG_ERR * (-1)) 	/* causes ng_bleat to write log messages too */
#define CRIT		(LOG_CRIT * (-1))

/* -------------------------------------------------------------------------------------------------- */
struct fsys {				/* stuff we need to understand about each file system */
	char	*name;
	long	count;			/* number of files here */
	long	nhld;			/* number of hlds allocated for this filesystem struct */
	long	hld_last;		/* last observed hld */
	long 	*hld_counts;		/* number of files in each hld's tree */
};


/* assume:  /ningaui/fs00/d1/d2/fname  */
struct stuff {			/* disect breaks the filename into this struct */
	char	fs[256];	/* the filesystem name */
	int	d1;		/* first directory name */
	int	d2;		/* second directory name */
} stuff;

/* -------------------------------------------------------------------------------------------------- */

char *version = "1.2/04299";

static	struct	fsys **fslist = NULL;		/* list of filesystems that we know about */
static	int	fsidx = 0;
static	int	nfslist = 0;			/* number allocated */
static	Symtab  *st;				/* small symbol tab used for quick lookup of fs names */
static	char	*indirect_refresh = NULL;	/* filename to read rm_db stuff from rather than invoking directly */
static	int	sld_max;			/* max files in a secondary level (or lower) directory (from RM_SLDMAX) */
static	int 	threshold=95;			/* point at which we expand directories */


/* -------------------------------------------------------------------------------------------------- */
/* 
	add more space to the hld list array  in fp 
	n will be the hld number so if we have 0, and we find /ning/fsxx/20 first, we will create 30 slots 
*/
static void extend_hldcounts( struct fsys *fp, int n )
{
	int old;		/* the old allocation value */

	old = fp->nhld;
	fp->nhld += n + 10;			/* get a few more than we may need */

	fp->hld_counts =  ng_realloc( fp->hld_counts, sizeof( long ) * fp->nhld, "fslist/subd" );
	memset( fp->hld_counts + old, 0, sizeof( long ) * (fp->nhld - old) );
}

/*
	looks into each highlevel directory and ensures that at least sld_max directories exist
	we dont care if there are more, but there must be at least this number so that a randomly
	chosen hld/sld pair by spaceman_req does not fail
*/
static void verify_sld( char *dname, int make )
{
	extern int errno;

	struct dirent 	*ent;		/* directory entry returned */
	DIR	*d = NULL;		/* pointer to open directory */
	int	i;
	int	missing = 0;
	int	*map = NULL;			/* map of things */
	char	buf[2048];
	int	x;

	map = (int *) ng_malloc( sizeof( int ) * sld_max, "map" );
	memset( map, 0, sizeof( int ) * sld_max );

	ng_bleat( 3, "verifying sld set for: %s", dname );

	if( (d = opendir( dname )) != NULL )
	{
		while( (ent = readdir( d )) != NULL )
		{
			if( isdigit( *ent->d_name ) )			  /* ignore insignificant file names */
			{
				x = atoi(ent->d_name);
				if( x >= 0 && x < sld_max )
					map[x]++;
				else
					ng_bleat( 0, "spaceman: directory name out of range: %s %d is not less than %d or is less than 0", dname, x, sld_max );
			}
		}

		closedir( d );

		for( i = 0; i < sld_max; i++ )
		{
			if( ! map[i] )
			{
				if( make )
				{
					sfsprintf( buf, sizeof( buf ), "%s/%02d", dname, i );
					ng_bleat( 0, "secondary level directory was missing: %s; created", buf );
					if( mkdir( buf, 0777 ) && errno != EEXIST )				/* bad if error and not because it existed */
						ng_bleat( ERROR, "verify: cannot make directory for: %s: %s\n", buf, strerror( errno ) );
				}
				else
					ng_bleat( 0, "secondary level directory missing: %s/%02d", dname, i );
			}
		}
	}
	else
	{
		sfprintf( sfstderr, "cannot open directory: %s: %s\n", dname , strerror( errno ) );
	}

	ng_free( map );
}

/* peeks into all of the directories in the filesystems we found - verifies each hld 
   has the right number of slds
*/
static void verify_hld( int make )
{
	extern int errno;

	struct dirent 	*ent;		/* directory entry returned */
	struct fsys	*fp;
	DIR	*d;			/* pointer to open directory */
	int	i;
	char	buf[2048];
	int	dnum;			/* directory number */
	int	mdnum = -1;		/* largest dir number we saw */
	int	ver_dnum[MAX_HLD];	/* verification of each hld -- prevent holes */
	int	nholes = 0;


	for( i = 0; i < fsidx; i++ )
	{
		if( (fp = fslist[i]) )
		{
			ng_bleat( 3, "verifying sld set for: %s", fp->name );

			if( (d = opendir( fp->name )) != NULL )
			{
				while( (ent = readdir( d )) != NULL )
				{
					if( isdigit( *ent->d_name ) )			  /* ignore insignificant file names */
					{
						if( (dnum = atoi( ent->d_name )) < MAX_HLD )	/* prevent insanity */
						{
							if( dnum > mdnum )
								mdnum = dnum;
							ver_dnum[dnum] = 1;			/* mark as existing for check later */

							sfsprintf( buf, sizeof( buf ), "%s/%s", fp->name, ent->d_name );
							verify_sld( buf, make );
						}
						else
							ng_bleat( 0, "hld name seems bad and was ignored: %s", ent->d_name );
					}
					else
						ng_bleat( 2, "invalid hld name was ignored: %s", ent->d_name );
				}

				closedir( d );

				ng_bleat( 2, "checking for hld holes in %s (%d)", fp->name, mdnum );
				nholes = 0;
				for( ; mdnum >= 0; mdnum-- )
				{
					if( ! ver_dnum[mdnum] )		/* didn't see this hld -- hole */
					{
						nholes++;
						ng_bleat( 0, "hld verify found hole%s: %s/%02d", make ? ", creating" : "", fp->name, mdnum );
						if( make )
						{
							sfsprintf( buf, sizeof( buf ), "%s/%02d", fp->name, mdnum );
							if( mkdir( buf, 0777 ) && errno != EEXIST )		/* bad if error and not because it existed */
								ng_bleat( ERROR, "cannot make hld directory: %s: %s\n", buf, strerror( errno ) );

							verify_sld( buf, make );		/* this will create the subdirs */
						}
					}
				}

				if( !nholes )
					ng_bleat( 2, "no holes found" );		/* match the checking for if we didn't generate any found messages */
			}
			else
			{
				ng_bleat( 0, "cannot open directory: %s: %s\n", fp->name, strerror( errno ) );
				exit( 1 );
			}
		}
	}
}

/* split up the file name from the rm_db dump. 
	We ASSUME that replication manager filesystems that we worry about all have a 
	'top' directory name of rmfsXXXX or fsXX; we'll skip all parts of the pathname
	upto that point.
		
		/directory/path/ya-da/ya-da/fsxx/hh/ll/filename

		hh -- highlevel directory number
		ll - low directory

   caller should snatch what it needs from the static buffer before calling again.
*/
static struct stuff *disect( char *b )
{
	char	*bp;		/* work pointer into the users buffer */
	char	*np;		/* pointer at next segment */
	int	i;
	int	done = 0;

	if( ! b )
		return NULL;

	bp = b;	

	while( !done )
	{
		bp++;
		if( bp = strchr( bp, '/' ) )
		{
			/*if( strncmp( bp, "/rmfs", 5 ) == 0 || ( *(bp+5) == '/' &&  *(bp+1) == 'f' && *(bp+2) == 's' && isdigit( *(bp+3) ) && isdigit( *(bp+4) ) ) )*/
			/* now need to handle ..../fsXX   and ...../rmfsXXXX  */
			if( (strncmp( bp, "/rmfs", 5 ) == 0 && isdigit( *bp+5 )) || (strncmp( bp, "/fs", 3 ) == 0 && isdigit( *bp+3 )) )
			{
				bp  = strchr( bp+1, '/' );
				done++;
			}
		}
		else
			return NULL;		/* smells like bad meat */

	}

	*bp = (char) 0;
	sfsprintf( stuff.fs, sizeof( stuff.fs ), "%s", b );		/* copy in the file system name */
	*bp = '/';			/* restore callers buffer */

 	bp++;
	stuff.d1 = atoi( bp );

	if( (bp = strchr( bp, '/' )) == NULL )
		stuff.d2 = -1;
	else
	{
		bp++;
		stuff.d2 = atoi( bp );
		if( strchr( bp, '/' ) == NULL )	/* must be one more to be legit */
			stuff.d2 = -1;
	}

	return &stuff;
}

/* show the state of our world to those curious enough to ask */
static void show( ) 
{
	int i;
	int j;
	int k;
	int pct_full;
	long tot = 0;
	char	dname[1024];

	if( verbose > 2 )
		return;

	for( i = 0; i < fsidx; i++ )
	{
		if( fslist[i] == NULL )
		{
			printf( "what? i=%d\n", i );
			return;
		}

		pct_full = 0;
		sfprintf( sfstdout, "%d %s files=%ld\n", i, fslist[i]->name, fslist[i]->count );

		if( fslist[i]->count )
		{
                        for( j = 0; j < fslist[i]->hld_last; j++ )
                        {
                                k = (fslist[i]->hld_counts[j]*100)/(sld_max*sld_max);			/* percentage of this hld that is used */
                                printf( "\t%s/%02d %d files (%d%%)\n", fslist[i]->name, j, fslist[i]->hld_counts[j], k );
                        }

			pct_full = (fslist[i]->count*100)/((sld_max*sld_max)*fslist[i]->hld_last);
			sfprintf( sfstdout, "\tpct full: %d%% (%ld/%ld)\n", pct_full, fslist[i]->count, (long) (sld_max*sld_max*fslist[i]->hld_last) );
		}
	}
}

/* add a filesystem block to the list */
static void fs_add( char *name )
{
	int	nhld;

	if( fsidx >= nfslist )
	{
		nfslist += 1024;
		fslist = (struct fsys **) ng_realloc( fslist, nfslist, "fslist in fs_add" );
		ng_bleat( 1, "allocated 1024 more fs pointers total=%d", nfslist );
	}

	fslist[fsidx] = (struct fsys *) ng_malloc( sizeof( struct fsys ), "fsys" );
	memset( fslist[fsidx], 0, sizeof( struct fsys ) );
	fslist[fsidx]->name = strdup( name );
	symlook( st, fslist[fsidx]->name, 0, (void *) fslist[fsidx], SYM_NOFLAGS );  /* for quick access */

	nhld = ng_spaceman_hldcount( name );			/* get real directory counts - dont depend on what rmdb says */
	extend_hldcounts( fslist[fsidx], nhld );		/* allocate these */
	fslist[fsidx]->hld_last = nhld;				/* mark the last one so we properly assess fullnes */
	fsidx++;
	ng_bleat( 2, "fs_add: %s(name) %d(nhld)", name, nhld );
}

/* get a list of filesystems that are under our control. arenad is 
   assumed to have n files. the first line of each file is the pathname 
   name of a file system that we are managing.  All regular files are 
   examined unless their names begin with the dot character.
*/
static void get_fslist( char *arenad )
{
	extern int errno;

	struct dirent 	*ent;		/* directory entry returned */
	DIR	*d;			/* pointer to open directory */
	char 	*buf;
	Sfio_t	*f;
	char	pname[255];

	if( ! arenad )
	{
		ng_bleat( CRIT, "get_fslist: cannot open arena directory" );
		exit( 1 );
	}

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
					sfprintf( sfstderr, "cannot open file: %s: %s\n", pname, strerror(errno) );
				}
			}
		}

		closedir( d );
	}
	else
	{
		sfprintf( sfstderr, "cannot open directory: %s: %s\n", arenad , strerror( errno ) );
		exit( 1 );
	}
}

/* reset the fsystem strucures in prep for reload of data */
static void reset( )
{
	int i;

	for( i = 0; i < fsidx; i++ )
	{
		fslist[i]->count = 0;
		if( fslist[i]->hld_counts )
			memset( fslist[i]->hld_counts, 0, sizeof( long ) * fslist[i]->nhld );
	}
}


/* reads info from rm_db and populates the fslist stuff */
static void refresh( )
{
	extern	int errno;
	static 	ng_timetype expires = 0;
	ng_timetype	now;
	ng_int64	used;
	Sfio_t	*f;			/* stream for command */
	struct stuff *p;		/* pointer at stuff from file name */
	char 	*buf;			/* pointer at input buffer */
	char	*tok;
	char	*fname;
	int	old;			/* last size of subd array (in rows)*/
	Syment	*se;			/* entry from symbol table */
	struct fsys	*fp;		/* pointer to entry we hash to */
	int		elapsed;	/* seconds required to get dump */
	char	tname[1024];		/* temporary file name */
	char	*tmp = NULL;
	char	cmd[1024];

	now = ng_now( );
	reset( );					/* clean out counts etc */

	/* expected output of ng_rm_db -p is: md5sum filename size */

	if( (tmp = ng_env( "TMP" )) == NULL )		/* we can work round TMP not being there, but its slow */
	{
		f = ng_sfpopen( NULL, "ng_rm_db -p", "r" );	/* open it as a pipeline cuz we cannot get tmp dir for file name */
		ng_bleat( 0, "could not get $TMP from cartulary, resorting to running rm_db and reading directly" );
	}
	else
	{
		sfsprintf( tname, sizeof( tname ), "%s/spaceman.%d", tmp, getpid() );
		sfsprintf( cmd, sizeof( cmd ), "ng_rm_db -p >%s", tname );
		ng_bleat( 1, "generating rm_db suff with command: %s", cmd );
		if( system( cmd ) )				/* failures are true */
		{
			ng_bleat( 0, "%s: command failed: %s", argv0, cmd );
			unlink( tname );
			return;
		}
		ng_bleat( 1, "rm_db command completed" );

		f = ng_sfopen( NULL, tname, "r" );
	}


	/*if( (f = sfpopen( NULL, "ng_rm_db -p", "r" )) != NULL ) */		/* get stuff from rep mgr */
	if( f != NULL )
	{
		long  rmdb_count = 0;

		while( (buf = sfgetr( f, '\n', SF_STRING )) != NULL )
		{
			rmdb_count++;

			tok = strtok( buf, " \t" );		/* md5 - discard */
			tok = strtok( NULL, " \t" );		/* file name */

			if( (p = disect( tok )) )			/* unravel the file name */
			{
				if( ! (se = symlook( st, p->fs, 0, 0, SYM_NOFLAGS )) )
				{
					fs_add( p->fs );				/* add it */
					ng_log( LOG_WARNING, "filesystem name not defined in arena list, ignored: %s", p->fs );
					se = symlook( st, p->fs, 0, 0, SYM_NOFLAGS );
				}

				fp = (struct fsys *) se->value;
				fp->count++;				/* total files on the system */

				if( p->d2 >= 0 )	/* good subdir name if d2 is set */
				{
					if( p->d1 >= fp->nhld )  			/* need more tracking space? */
						extend_hldcounts( fp, p->d1 );		/* give us the room we need */
					if( p->d1 > fp->hld_last )
						fp->hld_last = p->d1 + 1;			/* should not happen */

					fp->hld_counts[p->d1]++;				/* number of files neith the hld */
				}
			}
		}

		ng_bleat( 2, "refresh: rm_db request required %d seconds, rmdb_count=%I*d", (ng_now( ) - now)/10, Ii(rmdb_count) );

		if( sfclose( f ) )
			ng_bleat( 0, "errors reading process: ng_rm_db -p: %s", fname, strerror( errno ) );
	}
	else
	{
		ng_bleat( CRIT, "refresh: cannot read rm_db information from: %s: %s\n", indirect_refresh ? indirect_refresh : "call to rm_db -p", strerror( errno ) );
	}

	if( tmp )
		unlink( tname );
}


/* ensure sub directories below fsname exist; assume fsname exists */
static void mk_tier( char *fsname, int hld )
{
	extern int errno;
	char buf[1024];
	int i;


	sfsprintf( buf, sizeof( buf ), "%s/%02d", fsname, hld ); 
	ng_bleat( 1, "mk_tier: making high level directory:  %s", buf ); 

	if( mkdir( buf, 0777 ) && errno != EEXIST )				/* bad if error and not because it existed */
		ng_bleat( ERROR, "mk_tier: cannot make directory for: %s: %s\n", buf, strerror( errno ) );
	else
	{
		for( i = 0; i < sld_max; i++ )
		{
			sfsprintf( buf, sizeof( buf ), "%s/%02d/%02d", fsname, hld, i );
			ng_bleat( 1, "mk_tier: making second level directory: %s", buf );
			if( mkdir( buf, 0777 ) && errno != EEXIST )		/* this is bad */
				ng_bleat( ERROR, "mk_tier: cannot make directory for: %s: %s\n", buf, strerror( errno ) );
		}
	}
}

/* if we seem full on a set of directories (trying to limit the max per sld) then create a new hld */
static int check_fullness( int forreal )
{
	int i;
	int j;
	int k;
	long fcount;		/* total files in the file system */
	int pct_full;

	for( i = 0; i < fsidx; i++ )
	{
		if( fslist[i] == NULL )
		{
			ng_bleat( 0, "null pointer encountered in fslist at i=% fsidx=%d\n", i, fsidx );
			return;
		}

		pct_full = 0;
		fcount = 0;

		ng_bleat( 2, "checking fullness: %s  count=%I*d", fslist[i]->name, Ii(fslist[i]->count) );
		if( fslist[i]->count )
		{
			pct_full = (fslist[i]->count*100)/((sld_max*sld_max)*(fslist[i]->hld_last+1));

			if( pct_full > threshold )
			{
				if( forreal )
				{
					ng_bleat( 0, "filesystem is over threshold:  %s  %d(%%) %d(threshold)", fslist[i]->name, k, threshold );
					mk_tier( fslist[i]->name, fslist[i]->hld_last );
					fslist[i]->hld_last++;
				}
				else
					ng_bleat( 0, "filesystem is over threshold:  %s  %d(%%) %d(threshold); would make new tier (%s/%02d)", fslist[i]->name, pct_full, threshold, fslist[i]->name, fslist[i]->hld_last );
			}
			else
				ng_bleat( 1, "filesystem directory use ok: %s %d(%%) %d(threshold)", fslist[i]->name, pct_full, threshold );
		}
	}
}

static void usage( )
{
	sfprintf( sfstderr, "%s version %s\n", argv0, version );
	sfprintf( sfstderr, "usage: %s [-q] [-t threshold%%] [-v]\n", argv0 );
}

main( int argc, char **argv )
{
	int	i;
	int	j;
	int	k;
	int	query = 0;
	char	*tok;
	char	*pname;
	char	*arenad = NULL;

	ARGBEGIN
	{
		case 'q':	query = 1; break;
		case 'r':	SARG( indirect_refresh); break;
		case 't':	IARG( threshold ); break;
		case 'v':	OPT_INC( verbose ); break;
		default:
usage:
			sfprintf( sfstderr, "dont recognise: %s\n", *argv );
			usage( );
			exit( 2 );
	}
	ARGEND

	st = syminit( 51 ); 	/* allocate symbol table */

	if( (tok = ng_env( "RM_SLDMAX" )) == NULL )
	{
	 	ng_bleat( 0, "cannot find RM_SLDMAX in cartulary" );
		exit( 1 );
	}

	sld_max = atoi( tok );

	if( (arenad = ng_env( "RM_ARENA_ADM" )) == NULL )
	{
		sfprintf( sfstderr, "%s: cannot find RM_ARENA_ADM in cartulary/env\n", argv0 );
		exit( 1 );
	}


	ng_bleat( 1, "version %s started", version );

	get_fslist( arenad );		/* build list of filesystems in the arena */

	refresh( );			/* count files in each high level directory beneith each filesystem */
	if( query )
	{
		verify_hld( 0 );			/* verify the filesystems we discovered - but dont make anything */
		check_fullness( 0 );			/* see if we might make anything and report */
		show( );
		return 0;
	}

	verify_hld( 1 );			/* verify the filesystems we discovered; make missing directories if needed */
	check_fullness( 1 );
	
	return 0;
}



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_spaceman:Space Manager For The Replication Manager)

&space
&synop
	ng_spaceman [-q] [-t threshold-pctg] [-v]

&space
&desc	&This is responsible for scanning the &ital(replication manager) 
	filesystems, and making sure that everything is kept in order. 
	&This specifically looks at two things: The number of files in 
	each of the scondary level directories, and the number of 
	secondary level directories in each high level directory (e.g.
	/ningaui/fs00/01).  
&space
	The directory structure within an arena is assumed to be 
	organised in a two tier fashion (e.g. /ningaui/fs00/00/01/fn). 
	Each tier is assumed to be numeric, and start with &lit(00).
	A maximum of &lit(RM_SLDMAX) files or directories will be placed into 
	each of the tiers, with no restrictions on the number of 
	directories that will be defined at the "root" filesytem level. 
&space
	High level directories are added at the root (e.g. /ningaui/fs00)
	when the existing directories are determined to be &ital(full)
	(over the tollerance level).
	The &ital(fullness) of a directory is determined by calculating the 
	max number of files desired under the high level directory (&lit(RM_SLDMAX)
	squared) and comparing the actual number of files (as reported by 
	&lit(ng_rm_db) to the maximum. If the ratio of actual files to 
	maximumn files is larger than the tollerance percentage, then a 
	new high level directory, and its subdirectories, are created. 

&space
	&This also ensures that for each high level directory, all possible 
	secondary level directories exist. Thus, if &lit(RM_SLDMAX) is set 
	to 30, and &lit(/ningaui/fs00/02) has only &lit(00) and &lit(01)
	secondary level directories, then &this will attempt to create 
	directories &lit(02) through &lit(29.)
	Thus it is &stress(necessary) to run &this &stress(immediately)
	after increasing the value of &lit(RM_SLDMAX) to ensure that 
	all secondary level directories are present.
	This is necessary because &ital(ng_spaceman_req) randomly selects
	directory pathnames assuming that all of the secondary level directories
	exist. 

&space
	&This determines the filesystems that belong to the &lit(replication manager)
	by opening and reading the files contained in the directory referred to 
	with the &lit(NG_ARENA_ADM) cartulary variable. Files in this directory are 
	assumed to be named sequentially starting with &lit(00) and contain the pathname
	of the associated filesystem as their first line. The contents of the remainder 
	of these file is currently undefined, and ignored. 

&space
&subcat Cartulary Variables
	The following cartulary variables are expected to contain valuable 
	information for &this:
&space
&begterms
&term	RM_SLDMAX : Defines the maximum number of secondary level directories 
	that should exist beneith a high level directory, and the maximum
	number of files that should exist under a secondary level directory.
&space
&term	RM_ARENA_ADM : Specifies the directory path containing the 
	arena management files.
&endterms

&space
&opts	The following options are recognised by &this:
&begterms
&space
&term -q : Query mode. Causes &this to indicate the state of the arena, 
	but not to take any real action.

&term	-t : Defines the threshold (percentage) that will be used to determine 
	when new high level directories should be added. If this option is not 
	supplied, then a value of 95% is used. 
&space
&term -v :	Puts &this into verbose mode. 
&space
&endterms

&files	The following files are in some manner important to &this:
&space
&subcat	$NG_ROOT/log/spaceman
	It is assumed that &lit(ng_rota,) or what ever will execute &this, will 
	redirect its stdout and stderror to this file.
	
&subcat	Arena Admin Directory
	This directory (specified with the &lit(RM_ARENA_ADM) cartulary variable)
	contains one file per arena filesystem that &this is to manage. 
	&This only reads the first line of each file, and expects the 
	line to contain the root path of the filesystem. 
	All files in this directory are searched unless their file name 
	begins with a dot (.) character. 

&space
&subcat	Arena Filesystems
	Arena filesystems are assumed to have two nodes (i.e. &lit(/ningaui/fs01))
	and must be mounted with this name. 
&space
&see	ng_repmgr, ng_rm_db, ng_spaceman_nm

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	07 Jun 2001 (sd) : Brand new stuff.
&term	17 Aug 2001 (sd) : Converted to use bytes as file size to accomidate replication 
&term	11 Apr 2002 (sd) : Completely rewritten to its current state from the daemon that 
	it once was which supplied file pathnames via TCP/IP session.
	manager needs.
&term	04 Dec 2001 (sd) : To correct the order of stuff coming from ng_rm_db -p dump.
&term	29 Apr 2002 (sd) : To get the count of hlds from disk rather than repmgr. This keeps us 
	from thinking a fs is full until a newly allocated fs has been given a file.
&term	30 Apr 2002 (sd) : To redo the percentage calculation so that its done right.
&term	17 Sep 2004 (sd) : Now writes rm_db -p output to a flie and reads it so as not to 
	hold rm_db lock.
&term	08 Jun 2005 (sd) : Changed log message to indicate missing fs in arena list was ignored.
&term	08 May 2007 (sd) : Upped the max number of filesystems that it can handle. 
&term	19 Feb 2008 (sd) : Fixed bug that was causing new highlevel directories not to be made for
	rmfsXXXX filesystems.
&term	08 Dec 2008 (sd) : Found possible cause of occasional core dump.  If a directory name
	began with a digit and converted to an out of range integer... Fixed. (v1.1)
&term	27 Apr 2009 (sd) : The hld check now looks for 'holes' and corrects by adding missing 
	directories. 
&term	05 Oct 2009 (sd) : Corrected cause of core dump (line 575)
&endterms

&scd_end
------------------------------------------------------------------ */
