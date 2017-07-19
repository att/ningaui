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
 -------------------------------------------------------------------------------------
 Mnemonic:	spaceadj - Spaceman directory adjustment tool
 Abstract: 	Peeks into all replication manager filesystems and looks for directories
		that are out of balance. Schedules jobs to move files (within the filesystem)
		in an effort to keep directories balanced.
 Date:		20 April 2002 (hbd MKD/S)
 Author:	E. Scott Daniels
 -------------------------------------------------------------------------------------
#include	<ast_std.h>
*/
#include	<string.h>
#include	<time.h>
#include 	<inttypes.h>
#include	<syslog.h>
#include	<unistd.h>
#include	<stdio.h>
#include 	<sfio.h>
#include	<stdlib.h>
#include	<dirent.h>
#include	<errno.h>

#include	<ningaui.h>
#include	<ng_basic.h>
#include	<ng_lib.h>
#include	<ng_ext.h>
#include	<ng_tokenise.h>

#include	"spaceadj.man"


#ifdef KEEP 
+------------------------------+
 filesys                       |
	0		  nhld |
	[][][][][][+][+]...[+] |
		   |  |     |  |         0          max_sld
	           |  |     +----------->[n,n,n,n,...n]
	           |  +------------------->[n,n,n,n,...n]
                   |           |
	           +------------------->[n,n,n,n,...n]
+------------------------------+
#endif

#define FS_SPACE	1		/* file system name space */
#define SLD_SPACE	2		/* sld space in symtab */
#define FN_SPACE	3		/* filename space */

struct filesys {
	char	*name;
	int	ft;		/* total files in this file system */
	int	nhld;		/* number of hld directories */
	int 	**hlds;		/* list of 30 sub directories under each hld -- count of files in each */
};

typedef struct sld_t {			/* manage the file list in an sld */
	char	*name;		/* /ning/fs00/00/02 */
	int	nslots;		/* number of slots allocated */
	char	**slots;	/* list of file names (full paths) */
	int	idx;		/* index into slots - next to use */
	int	ridx;		/* next slot for a pull */
} sld_t;

int	max_sld = 30;
int	tol_sld = 10;		/* # files we allow a dir to go over max_sld before balancing -- prevent thrashing as things naturally move */
Symtab	*symtab = NULL;


/* --------------------------------------------------------------------- */
static 	struct filesys *newfs( char *name )
{
	struct filesys *fs;
	int 	i;

	fs = (struct filesys *) malloc( sizeof( struct filesys ) );
	memset( fs, 0, sizeof( struct filesys) );

	fs->name = strdup( name );
	fs->nhld = ng_spaceman_hldcount( name );			/* number of hlds currently in the filesystem */
	fs->hlds = (int **) malloc( sizeof( int * ) * fs->nhld );

	for( i = 0; i < fs->nhld; i ++ )
	{
		fs->hlds[i] =  (int *) malloc( sizeof( int ) * max_sld );
		memset( fs->hlds[i], 0, sizeof( int ) * max_sld );
	}

/*	sym_map( symtab, name, FS_SPACE, fs );	at home*/
	symlook( symtab, name, FS_SPACE, fs, SYM_COPYNM );

	return fs;
}

sld_t *newsld( char *name )
{
	sld_t	*sp;

	sp = (sld_t *) ng_malloc( sizeof( sld_t ), "sld_t" );
	
	memset( sp, 0, sizeof( sld_t ) );
	sp->name = strdup( name );

	/*sym_map( symtab, name, SLD_SPACE, sp );    at home*/
	symlook( symtab, name, SLD_SPACE, sp, SYM_COPYNM );

	return sp;
}

static void *sym_get( Symtab *st, char *name, int space )
{
	Syment	*se = NULL;

	if( (se = symlook( st, name, space, 0, SYM_NOFLAGS )) != NULL )
		return se->value;

	return NULL;
}

/*	 add a file to our environment 
	assume: /ningaui/fs00/hld/sld/filename
*/
static void addfile( char *fname, char *md5 )
{
 	sld_t 	*sp;	
	struct	filesys *fs;
	char 	*tok;
	char	**tlist = NULL;
	int	tcount;
	int	hld;
	int	sld;
	char	fsname[256];

	tok = strrchr( fname, '/' );		/* find end of the path */

if( tok - fname <18 )				/* rm_db seems to be returning /ningaui/fs00/00/00name */
{
	ng_bleat( 0, "skipped: %s; bad format", fname );
	return;
}

	*tok = 0;				/* temp mark just the path name */
	if( (sp = (sld_t *) sym_get( symtab, fname, SLD_SPACE )) == NULL )
		sp = newsld( fname );
	*tok = '/';
	if( sp->idx >= sp->nslots )
	{
		sp->nslots += max_sld;
		sp->slots = realloc( sp->slots, sp->nslots * sizeof( char *) );
	}

	if( sym_get( symtab, fname, FN_SPACE ) )		/* second instance of this full path -- not good */
	{
		ng_bleat( 0, "ERROR: multiple instances of same path: %s", fname );
		symdel( symtab, fname, FN_SPACE );		/* prevent from moving it  as we cnnot be sure what is right */
		return;
	}

	sp->slots[sp->idx] = strdup( fname );
	symlook( symtab, sp->slots[sp->idx], FN_SPACE, strdup( md5 ), SYM_NOFLAGS );		/* save the md5 */
	sp->idx++;
	
			
	if( (tlist = ng_tokenise( fname+1, '/', '\\', tlist, &tcount )) != NULL )
	{
		int	*list;
		int	i;

		hld = atoi( tlist[2] );
		sld = atoi( tlist[3] );
		sprintf( fsname, "/%s/%s", tlist[0], tlist[1] );

		if( (fs = (struct filesys *) sym_get( symtab, fsname, FS_SPACE )) == NULL )
			fs = newfs( fsname );


		if( (list = fs->hlds[hld]) == NULL )
		{
			sfprintf( sfstderr, "unexpected null in hlds\n" );
			exit( 1 );
		}

		fs->ft++;		/* total files in the file system */
		list[sld]++;		/* count number of hits in each sld */
		
		free( tlist );
	}
}

/* find the next file from the hld/sld block to move */
static char *getfile( struct filesys *fs, int hld, int sld )
{
	sld_t	*sp;
	char	buf[2048];
	char	*fname = NULL;
	int	i;

	
	sprintf( buf, "%s/%02d/%02d", fs->name, hld, sld );		/* name of the sld that we will pull a file from */
	if( (sp = (sld_t *) sym_get( symtab, buf, SLD_SPACE )) != NULL )
	{
		/* find first slot that is not null, an that the name does NOT have a plus (attempt number) */
		for( i = sp->ridx; i < sp->idx && sp->slots[i] == NULL || strchr( sp->slots[i], '+' ); i++ );
		if( i < sp->idx )
		{
			fname = sp->slots[i];
			sp->slots[i] = NULL;
			sp->ridx = i + 1;
		}
	}
	else
		ng_bleat( 0, "could not find sld in symtab: (%s)", buf );

	return fname;
}

/* 
	ensure that the same basename in the dest hld/sld does not exist. if
	it does not then putout the information to move the file.
	return 1 if we can move the file, zero otherwise.

	output is md5 size srcfile destfile 
*/
static int mvfile( Sfio_t *mf, struct filesys *fs, char *ofname, int hld, int sld )
{
	
	char	*tok;
	char 	*fname;
	char	*md5;		/* md5/size info squirreled away when we found the file in the flist */
	char	buf[2048];

	fname = strrchr( ofname, '/' );
	fname++;

	
	sfsprintf( buf, sizeof( buf ), "%s/%02d/%02d/%s", fs->name, hld, sld, fname );		/* destination filename */
	if( sym_get( symtab, buf, FN_SPACE ) == NULL )						/* file not in this directory */
	{
		md5 = (char *) sym_get( symtab, ofname, FN_SPACE );
		if( md5 == NULL )
		{
			ng_bleat( 3, "\tfile not moved: no md5 for source file: %s", ofname );
			return 0;
		}

		sfprintf( mf, "%s %s %s\n", md5, ofname, buf );

		return 1;
	}
	else
		ng_bleat( 1, "\tnot moved collision detected: %s -> %s", ofname, buf );
	
	return 0;
/*	sfprintf( mf, "%s %s/%02d/%02d/%s\n", ofname, fs->name, hld, sld, fname );*/
}

/* remove amt files from srch/srcs and add to hld/slds that have room */
static int del( Sfio_t *mf, struct filesys *fs, int fd, int amt, int srch, int srcs )
{
	static int last_hld=0;
	static int last_sld=0;

	int	i;
	int	*sld;
	int	moved = 0;
	int	lmoved;			/* moved into current directory */
	int	nadd; 			/* number to add at the sld */
	char	*ofname;		/* old file name */

	if( ! fs )			/* assume a reset of things */
	{
		last_hld = 0;
		last_sld = 0;
		return 0;
	}

	if( ! fs )
		return -1;

	ng_bleat( 2, "del: %s/%02d/%02d(from) %d(amt) %d(fd) (%d,%d) %d(nhld)", fs->name, srch, srcs, amt, fd, last_hld, last_sld, fs->nhld );

	for( ; last_hld < fs->nhld && amt > 0; last_hld++ )
	{
		sld = fs->hlds[last_hld];
		
		for( ; sld && last_sld < max_sld; last_sld++ )
		{
			if( (nadd = fd - sld[last_sld]) > 0 )			/* space at this sld */
			{
				lmoved = 0;					/* number moved to this target */
				nadd = nadd < amt ? nadd : amt;
				ng_bleat( 3, "\ttrying: %02d/%02d %d(nadd) %d(amt) %d(sld) %d(calc-after)", last_hld, last_sld, nadd, amt, sld[last_sld], sld[last_sld]+nadd );

#ifdef BEFORE_MD5
				amt -=  nadd;
				moved +=  nadd;
				sld[last_sld] += nadd;
				for( i = 0; i < nadd; i++ )
				{
					if( (ofname = getfile( fs, srch, srcs)) != NULL )
						mvfile( mf, fs, ofname, last_hld, last_sld );			/* add file pair (src/dest) to the move list */
				}
#endif
				while( nadd && (ofname = getfile( fs, srch, srcs)) != NULL )
				{
					if( mvfile( mf, fs, ofname, last_hld, last_sld ) )			/* move could be done */
					{
						amt--;
						lmoved++;
						moved++;
						sld[last_sld]++;
						nadd--;
					}
				}

				if( lmoved )
					ng_bleat( 2, "\tadded: %02d/%02d %d %d(after)", last_hld, last_sld, lmoved, sld[last_sld] );
			}
			else
					ng_bleat( 3, "\tskip   %02d/%02d %d(nadd) %d(amt) %d(sld)", last_hld, last_sld, nadd, amt, sld[last_sld] );

			if( amt == 0 || ofname == NULL )		/* out of things to try, or moved enough already */
			{
				ng_bleat( 2, "\t%d(moved)", moved );
				return moved;
			}

			if( amt < 0 )
			{
				ng_bleat( 1, "\n==>>>something smells amt < 0 hld=%d sld=%d", last_hld, last_sld );
			}
		}
	
		last_sld = 0;
	}

	ng_bleat( 2, "\t%d(moved)", moved );		/* less than requested amount could be moved */
	return moved;	
}


static void fsdump( struct filesys *fs, int fd )
{
	ng_bleat( 1, "%s(name) %d(ft) %d(nhld) %d(fd)", fs->name, fs->ft, fs->nhld, fd );
}

/* called as we traverse the symtab for each filesystem. generates the list of files to be moved */
void spaceadjust(  Syment *se, void *data )
{
	Sfio_t	*mf;		/* move list file */
	struct filesys *fs;
	int 	fd;		/* optimum (balanced) files per sld ft/(nhld*maxsld) */
	int	tfd;		/* tollerated 'overflow' of files in a directory (fd+10) */
	int	h;
	int	s;
	int	*sld;		/* pointer at secondary file counts */
	int	moved;
	int	tmoved = 0;	/* total moved */
	char	*name;
	int total = 0;


	name = se->name;
	if( (fs = (struct filesys *) se->value) == NULL )
	{
		ng_bleat( 0, "spaceadjust: got null fs pointer!" );
		return;
	}

	if( (mf = (Sfio_t *) data) == NULL )
	{
		ng_bleat( 0, "spaceadjust: got null output file pointer!" );
		return;
	}

	del( mf, NULL, 0, 0, 0, 0 );			/* reset del's statics before we get started */

	fd = fs->ft/(fs->nhld * max_sld);		/* files desired in each directory for balance */
	fd++;						/* because its likely an irrational number */
	tfd = fd + tol_sld;				/* we will prune a directory if it has more than tfd files */
	if( fd > max_sld )				/* spaceman should have created enough directories -- this is an issue */
		ng_bleat( 1, "fd of %d smells!", fd );

	ng_bleat( 1, "adjusting fs: name=%s %d(ft)/(%d(maxsld)*%d(nhld)) = %d(fd)", fs->name, fs->ft, max_sld, fs->nhld, fd );

	for( h = 0; h < fs->nhld; h++ )		/* generate a list of files to move, and the destination -- to be read by ng_shuffle */
	{	
		sld = fs->hlds[h];
		for( s = 0; sld && s < max_sld; s++ )
		{
			if( tfd - sld[s] < 0 )						/* too many files in this sld */
			{
				ng_bleat( 2, "adjusting: %02d/%02d %d(had)", h, s, sld[s] );
				moved = del( mf, fs, fd, sld[s] - fd, h, s );		/* delete from this h/s by generating move pairs */
				sld[s] -= moved;
				tmoved += moved;
				ng_bleat( 2, "adjusted: %02d/%02d %d(now)\n", h, s, sld[s] );
			}
			else
				if( (sld[s] && verbose > 2) || verbose > 3 )
					ng_bleat( 2, "no adjust: %02d/%02d %d(had)\n", h, s, sld[s] );
		}
	}

	for( h = 0; h < fs->nhld; h++ )		/* this is purely verification verbose output - state of things after the moves */
	{	
		sld = fs->hlds[h];
		if( ! sld )
			ng_bleat( 0, "*** warning *** sld is null %d/%d", h, s );

		for( s = 0; sld && s < max_sld; s++ )
		{
			if( sld[s] || verbose > 2 )
				ng_bleat( 2, "%s/%02d/%02d %d(files) %d(adjust) ", fs->name, h, s, sld[s], fd - sld[s] );
			total += sld[s];
		}
	}

	ng_bleat( 1, "%s(name) %d(total) %d(fsft) %d(maxsld) %d(fd) %d(nhld) %d(moved)\n", fs->name, total, fs->ft, max_sld, fd, fs->nhld, tmoved );
}

static void usage( )
{
	sfprintf( sfstderr, "usage: %s [-n] [-s max_sld] [-t tollerance] [-v]\n", argv0 );
}

int main( int argc, char **argv )
{
	Sfio_t	*f = NULL;			/* file (pipe) to read filelist from */
	Sfio_t	*mf = NULL;			/* pointer to move list file (output) */
	char	*input;				/* pointer to buffer from the filelist pipe */
	char	*fsize;				/* pointer to the file size token */
	char	*md5;				/* pointer to md5 token */
	char	*fname;				/* pointer to fname token */
	int	fnum = 0;
	int	forreal = 1;		/* turn off (-n) if we just want to pretend */
	char	*tok;
	char	mvlistf[2048];			/* name of the move list file */
	char	buf[2048];

	symtab = syminit( 49999 );

	if( (tok = ng_env( "RM_SLDMAX" )) != NULL )
	{
		max_sld = atoi( tok );
		free( tok );
	}
	else
		ng_bleat( 0, "could not find RM_SLDMAX in cartulary; using %d as default", max_sld );


	ARGBEGIN
	{
		case 'n': forreal = 0; break;
		case 's': IARG( max_sld ); break;
		case 't': IARG( tol_sld ); break;
		case 'v': verbose++; break;
		default:
usage:
		usage( );
		exit( 1 );
	}
	ARGEND

	if( forreal )
		sfsprintf( mvlistf, sizeof( mvlistf ), "/tmp/mvlistf.%d", getpid( ) );
	else
		strcpy( mvlistf, "/dev/fd/1" );				/* generate to standard output */
	
	if( (f = sfpopen( NULL, "ng_rm_db -p", "r" )) == NULL )				/* dump the local file list database */
	{
		ng_bleat( 0, "unable to open file: %s: %s", "/tmp/daniels.test", strerror( errno ) );
		exit( 1 );
	}

	while( (input = sfgetr( f, '\n', SF_STRING )) != NULL )
	{
		md5 = strtok( input, " " );
		fname = strtok( NULL, " " );
		fsize = strtok( NULL, " " );
		if( fsize != NULL )			/* all three tokens found */
		{
			sfsprintf( buf, sizeof( buf ), "%s %s", md5, fsize );
			addfile( fname, buf );
		}
		else
			ng_bleat( 0, "bad record? %s(md5) %s(fname) NULL(size)", md5, fname );
	}

	if( (mf = ng_sfopen( NULL, mvlistf, "w" )) == NULL )
	{
		ng_bleat( 0, "unable to open mvlist file: %s: %s", mvlistf, strerror( errno ) );
		exit( 1 );
	}

	/*sym_foreach_class( symtab, FS_SPACE, spaceadjust, NULL );*/
	symtraverse( symtab, FS_SPACE, spaceadjust, mf );			/* adjust each filesystem that we saw */

	if( sfclose( mf ) )
	{
		ng_bleat( 0, "write errors to %s: %s", mvlistf, strerror( errno ) );
		exit( 1 );
	}

	if( forreal )			/* submit the command to do the moves */
	{
		sfsprintf( buf, sizeof( buf ), "ng_wreq job : pri 70 cmd ng_shuffle -p -v %s", mvlistf );
		ng_bleat( 1, "submitting shuffle job: %s", buf );
		system( buf );
	}

	return 0;
}


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_spaceadj:Spacman Directory Adjuster)

&space
&synop	ng_spaceadj [-n] [-s max_sld] [-t tollerance] [-v]

&space
&desc	&This requests a list of files from the &ital(replication manager) 
	database (rm_db) and determines if any of the directories in any of the 
	filesystems are out of ballance. Using the number of high level directories
	and the current number of files the &ital(balanced directory size) is 
	calculated for the filesystem. If &lit(ng_spaceman) has been creating the 
	proper number of highlevel directories, this value will be less than the 
	value &lit(RM_SLDMAX.) Any directory having more files than the balanced
	size will be pruned by moving files from that directory to directories 
	that are currently under the balanced size. 
&space
	Pruning and movement of files is done by creating a list containing the
	source filename, destination filename, and MD5 checksum value. This list
	is provided to &lit(ng_shuffle) to actually do the movement and registration 
	changes in &ital(replication manager.)

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-n : No action. The environment is looked at, and the move list generated, 
	but the list is not submitted for action.
&space
&term	-s n : Allows the user to supply the number of second level directories that 	
	will be created under each high level directory.  This is also the desired
	number of files within each second level directory.
	If not supplied the default value is the value of the &lit(RM_SLDMAX) cartulary
	variable.
&space
&term	-t n : Inidcates a tollerance value (n).  A directory containing more than the 
	balanced size number of files will be tollerated (not pruned) if the extra files
	in the directory do not exceed &ital(n.)  This should prevent thrashing as it is 
	expected that the normal flow of files should keep directories within 
	tollerance.  If not supplied, this value defaults to 10.
&space
&term	-v : Verbose mode. The more -v options the chattier (to a point).
&endterms

&space
&exit

&space
&see	ng_spaceman, ng_spaceman_mv ng_repmgr, ng_shuffle

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 	24 Apr 2002 (sd) : Original code.
&term	16 Nov 2006 (sd) : Converted sfopen() to ng_sfopen().
&term	19 Dec 2006 (sd) : Fixed man page so that it generates now.
&endterms

&scd_end
------------------------------------------------------------------ */
