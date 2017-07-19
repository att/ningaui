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
 ------------------------------------------------------------------------------
 Mnemonic:	collate
 Abstract:	Manages jobs that are submitted to senechal to 'roll-up' files
		to a desired node.
		Looks a bit like nawab, though we could not build a nawab
		programme to do this because of the issues revolving round restart
		if even a single job fails.
 Date:		22 April 2003
 Author:	E. Scott Daniels 
 ------------------------------------------------------------------------------
*/

#include	<sys/types.h>
#include	<sys/stat.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<fcntl.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<string.h>
#include 	<signal.h>
#include	<errno.h>

#include	<sfio.h>

#include	"ningaui.h"
#include	"ng_lib.h"
#include	"ng_tokenise.h"

#include 	"collate.man"

#define VFLAG	(verbose > 2 ? "-v" : "")

#define	JOB_SPACE	1			/* symbol table name spaces */
#define FILE_SPACE	2
#define NODE_SPACE	3

#define JS_OK		0			/* job states */
#define JS_PENDING	1
#define JS_RUNNING	2
#define JS_FAILED	3
#define JS_USED		4			/* output from job was rolled up */

#define JF_UNREP	0x01			/* job flags -- unreplicate the jobs output as its an intermediate */

#define NS_OK		0			/* node states */
#define NS_FAILED	1

#define FS_NEED		0			/* file states, need to roll */
#define FS_PENDING	1			/* added to a job somewhere */
#define FS_HAVE		2			/* its job was successful */

#define KEEP_LISTS	1			/* values for keep var -- keep the file lists given to the jobs -k sets */
#define KEEP_INTF	2			/* keep lists and intermediate files -kk or -k2 or -k -k sets */

typedef struct Node_t Node_t;

typedef struct File_t {				/* info about a file that is to be collated */
	char	*name;
	int	state;				/* have, need, pending */
	char	**nodes;			/* list of nodes this puppy is on */
	int 	*nattempt;			/* flags each node that we have attempted this on */
	char 	**path;				/* full path of each file on the appropriate node */
	int	nnodes;				/* number allocated */
	int	idx; 				/* index into nodes */
	ng_int64 size;				
} File_t;

typedef struct Job_t {
	char	name[256];
	Node_t	*node;				/* node to which it is assigned */
	int	state;				/* pending, running, ok, failed */
	char	*ofname;			/* name of output file that job produces */
	char	*lfname;			/* name of the list file that this job uses */
	Sfio_t	*lf;				/* pointer to the open list file */
	int	flags;				/* JF_ constants */
	int	fidx;				/* index into file list (points at last+1) */
	int	nfiles;				/* number of file pointers allocated */
	File_t	**files;			/* files collating with this job */
	int	phase;				/* phase it was created for -- phase 1 jobs dont have intermediate files to keep */
} Job_t;

struct Node_t {
	char 	*name;
	int	state;				/* good or bad */
	long	count;				/* number of files being processed here */
	Job_t	*job;				/* job being built for this node */
	ng_int64 size;				/* size of files assigned to the node */
};

Sfio_t	*bleatf = NULL;				/* where we bellow when running as a daemon */
Symtab	*symtab = NULL;				/* reference to just about everything */
long	running_jobs = 0;			/* number of jobs still pending in seneschal */
long	failed_jobs = 1;			/* this is set to force us to 'resubmit' jobs the first time round (sneaky!)*/
long	unique_id = 0;				/* unique id for job names */
int	timeout = 0;				/* max seconds to wait for jobs to finish */
int	priority = 75;				/* jobs are submitted with this priority */
int 	passthru = 0;				/* rollup jobs are generated with -P option to pass the list in */
int	assign_by_count = 1;			/* assign files to jobs based on count rather than size; -s sets to 0 indicating size */
int	sort_by_size = 1;			/* assign files to jobs based on size; sort list first  -S turns this and assign-by-count off */
ng_int64 size_scale = 1024 * 1024; 			/* sizes are converted to Mb by default */
int	attempts = 1;				/* number of attempts seneschal can make */
int	phase = 0;				/* what phase of processing, phase1 the output file is the user supplied file */
int	keep = 0;				/* defines what intermediate list/files should be kept at end of run */
int	rm_fs_full = 1;				/* value of NG_RM_FS_FULL to pass to rollups on remote nodes */
char	*resource = "";				/* optional seneschal resource */ 
ng_timetype expiry = 0;				/* when jobs expire because of a timeout */
char	*mynode = NULL;
char	*target_node = NULL;			/* where fiinal files should be rolled up */

char	*usr_cmd = NULL;			/* command that needs to be run on each node to roll things up */
char	*final_cmd = NULL;			/* command for the finall roll of all intermediate files */
char	*usr_output = NULL;			/* filename user wants rolled up stuff in -- ofile in phase 1 */
char	*final_method = NULL;			/* different method run for last rollup. if not supplied method is run in phase 1 */

/* ---------------- prototypes ----------------------------- */
char *build_cmd( int argc, char **argv, char *method, char *outfile );		/* builds the user command from parms passed in */
void parse_status( char *msg );
int job_maint( );
void unreplicate( char *fname );			/* remove a file from the replication managed space */

Job_t *mkjob( Node_t *n );
void add2list( Job_t *j, File_t *f, char *full_name );	/* add file to the list of stuff */
void purge_job( Job_t *j );
void submit_job( Syment *se, void *data );		/* submit the job -- invoked by traverse */
void add_output( Syment *se, void *data );		/* add output to need file list */
void clean_job( Syment *se, void *data );		/* general end of execution cleanup that is needed on a job basis */
void failjob( Syment *se, void *data );			/* dummy up a failure message and call parse status for each running */
void convert_job( Syment *se, void *data );		/* converts job state according to int tuple pointed to by data */
void file_sort( File_t *fp );				/* manages a list of files (disk) and will drive the assignment process if fname is null */

Node_t *select_node( File_t *f, char **path );		/* finds a node for the job */
Node_t *add_node( char *name );
Node_t *get_node( char *name );				/* get a node, creates if its not there */

void assign_files( Syment *se, void *data );		/* assigns a file to a job -- called by symtraverse */
void reset_files( File_t **flist, int n, int state );	/* change the state of a file */


#include	"coll_netif.c"				/* our specialised network interface stuff */

/* ---------- misc ----------------------------------------------------------------------------------*/
/*
	submits an rm_register -u command for the file.
*/
void unreplicate( char *fname )
{
	char	*bp;		/* pointer to the basename of the output file */
	char	cmd[NG_BUFFER];

	if( !fname )
		return;

	bp = strrchr( fname, '/' );		/* find basename */
	if( bp )
		bp++;
	else
		bp = fname;

	sfsprintf( cmd, sizeof( cmd ), "ng_wrapper --detach ng_rm_register -u -f %s \">/dev/null 2>&1\"",  bp );
	ng_bleat( 2, "unreplicate: %s", cmd );
	system( cmd ); 
}

/*	build a command from method supplied as argc/argv or as one string in method
	if outfile is passed in, we assume that its a fully qualified path that 
	is 'hard coded' on the command line rather than given to seneschal as 
	a basename.  The register flag on the ng_rollup command is then not set
	either.

	If the user did not give a final method, we save the original one so that 
	for the last pass we can rebuild the command with the same method.  We used
	to save the command, but in order to write to a non-replicated file, we need
	to rebuild the command at the end, so saving the original method was needed. 
 */
char *build_cmd( int argc, char **argv, char *method, char *outfile )
{
	static char	*ng_root;

	char	cmd[NG_BUFFER];			
	char	mbuf[2048];			/* we build method here if argc/arv passed in */
	int	space;				/* space left in the buffer */
	int	l;
	char 	cmd_term[1024];			/* the last part of the cmd with output and redirection */

	if( outfile )
		sfsprintf( cmd_term, sizeof( cmd_term ), "\" %s", outfile );	/* assume not a registered file */
	else
		sfsprintf( cmd_term, sizeof( cmd_term ), "\" %%o1" );	/* outfile given as a seneschal file for attempt number modification (actual file name sent in seneschal job request) */
	
	strcat( cmd_term, " >/tmp/rollup_start.log 2>&1" ); 	/* add redirection to output info */

	space = sizeof( cmd ) - (strlen( cmd_term ) +2);		/* reserved space at the end for the output file and stuff */

	if( ! ng_root && ! (ng_root = ng_env( "NG_ROOT" )) )
	{
		ng_bleat( 0, "cannot find NG_ROOT in cartulary/environment" );
		exit( 1 );
	}


	/*sfsprintf( cmd, space, "ng_rollup -j %%j %s -f %%i1 -L %s/tmp -l -v -d -r -t %s -m \"", passthru ? "-P" : "", ng_root, target_node ); */
	sfsprintf( cmd, space, "NG_RM_FS_FULL=%d ng_rollup -j %%j %s %s -f %%i1 -L %s/tmp -l -v -d -t %s -m \"", rm_fs_full, passthru ? "-P" : "", outfile ? "" : "-r", ng_root, target_node ); 
	space -= strlen( cmd ) + 1;

	if( method == NULL )
	{
		*mbuf = 0;
		while( argc && (space > (l=strlen( *argv ) + 1)) )		/* method comes in as argc/argv from command line for all but final */
		{
			argc--;
/*
		strcat( cmd, *argv );
		strcat( cmd, " " );
*/
			strcat( mbuf, *argv );
			strcat( mbuf, " " );
			space -= l;
			argv++;
		}

		if( !final_method )
			final_method = strdup( mbuf );		/* no final method; we default to original method so we can rebuld command at end */

		if( argc )					/* still arguments to process */
		{
			ng_bleat( 0, "abort: method too long: %d bytes, had room for %d", strlen( method ), space );
			exit( 1 );
		}

		strcat( cmd, mbuf );					/* add to buffer */
	}
	else
	{
		if( strlen( method ) < space )			/* final method passed in as option */
			strcat( cmd, method );
		else
			ng_bleat( 0, "method too long: %d bytes, had room for %d", strlen( method ), space );
	}

	strcat( cmd,  cmd_term );				/* add termination stuff like redirection */
	ng_bleat( 2, "command built: %s", cmd );

	return ng_strdup( cmd );
}

/* ---------- node stuff ----------------------------------------------------------------------------*/

Node_t *add_node( char *name )
{
	Node_t *n;

	n = (Node_t *) ng_malloc( sizeof( *n ), "add_node" );
	n->state = NS_OK;
	n->name = ng_strdup( name );
	n->job = NULL;
	n->count = 0;
	n->size = 0;
	symlook( symtab, n->name, NODE_SPACE, n, SYM_NOFLAGS );

	ng_bleat( 3, "add_node: %s added", n->name );
	return n;
}

Node_t *get_node( char *name )
{
	Syment *se;

	if( (se = symlook( symtab, name, NODE_SPACE, 0, SYM_NOFLAGS )) != NULL )
		return (Node_t *) se->value;
	else
		return add_node( name );
}

/* based on the nodes where a file lives, get one that we have not tried yet 
	try to balance better if we can
*/
Node_t *select_node( File_t *f, char **path )
{
	Node_t *n;
	Node_t *best = NULL;	/* best node we find */
	int bestn = 0;		/* index into nattempt of best node we find */
	int i;

	for( i = 0; i < f->idx; i++ )
	{
		if( ! f->nattempt[i] ) 		/* not attempted on this node yet */
		{
			n = get_node( f->nodes[i] );
			if( n->state == NS_OK )		/* node still in an ok state */
			{
				if( assign_by_count )
				{
					if( ! best || n->count < best->count )	/* this node has less assigned -- assign to it */
					{
						best = n;		/* current winner */
						bestn = i;		/* save index into f->nodes */
					}
				}
				else
				{
					if( ! best || n->size < best->size )	/* this node has less assigned -- assign to it */
					{
						best = n;		/* current winner */
						bestn = i;		/* save index into f->nodes */
					}
				}
			}
			else
			{
				f->nattempt[i]++;		/* prevent look up next time */
				ng_bleat( 4, "select_node: skipped b/c node error %s(node) %s(file)", f->nodes[i], f->name );
			}
		}
	}	

	if( best )
	{
		f->nattempt[bestn]++;
		if( path )			/* caller wants pointer to path back too */
			*path = f->path[bestn];
	
		best->count++;
		best->size += f->size; 
		ng_bleat( 4, "select_node: %s(node) %s(file) %I*d(sz)", best->name, f->name, Ii(best->size) );
		return best;
	}

	ng_bleat( 2, "select_node: none available  %s(file) %d", f->name, f->idx );
	return NULL;
}

/* ---------- file stuff ----------------------------------------------------------------------------*/

/* manage files on disk with sizes and when called with null file pointer, sort the list and drive the 
   job assigment process in sort order
*/
void file_sort( File_t *fp )
{
	static Sfio_t *sfile = NULL;		/* list that we will give to sort */
	static good_state = 1;			/* we set to 0 to indicate bad sort file */
	static char *fname = NULL;

	char	buf[2048];
	char	*rec;
	char	*tmp;
	char	*tok;
	Syment	*se;
	
	if( !fp )			/* sort the file names we've collected then drive the assignment */
	{
		if( good_state && sfile != NULL )
		{
			if( sfclose( sfile ) == 0 )
			{
				sfsprintf( buf, sizeof( buf ), "sort -k 1rn,1 %s", fname );
				ng_bleat( 3, "file_sort: %s", buf );
				if( (sfile = sfpopen( NULL, buf, "r" )) != NULL )
				{
					while( (rec = sfgetr( sfile, '\n', SF_STRING )) )
					{
						tok = strchr( rec, ' ' ) + 1;		/* skip size */
						if( (se = symlook( symtab, tok, FILE_SPACE, 0, SYM_NOFLAGS )) )
							assign_files( se, NULL );		/* simulate a traverse */
					}		

					sfclose( sfile );
				}
				else
				{
					ng_bleat( 0, "panic: file_sort: cannot read sorted data" );
					exit( 1 );
				}

				good_state = 0;	
				return;
			}

			unlink( fname );
			ng_free( fname );
			fname = NULL;
		}
		else			/* not a good state, just go the old way */
		{
			symtraverse( symtab, FILE_SPACE, assign_files, NULL );		/* assign files to jobs */
		}
		return;
	}

	if( !good_state )
		return;

	if( sfile == NULL )
	{
		if( (tmp = ng_env( "TMP" )) == NULL )
			tmp = "/tmp";
		sfsprintf( buf, sizeof( buf ), "%s/PID%d.collate.sfile", tmp, getpid() );
		fname = strdup( buf );
		ng_bleat( 2, "file_sort: creating external sort file: %s", fname );

		if( (sfile = sfopen( NULL, fname, "w" )) == NULL )
		{
			good_state = 0;
			return;
		}
	}	

	sfprintf( sfile, "%I*d %s\n", Ii(fp->size), fp->name );
}

/* more for testing than for practiacl use */
void dump_file( Syment *se, void *data )
{
	File_t *f;
	Node_t *n;
	int i;
	char *s;
	char *p;

	s = (char *) data;
	
	if( se && (f = (File_t *) se->value) )
	{
		sfprintf( sfstderr, "%s: %s(name) ", s, f->name );
		n = select_node( f, &p );
		sfprintf( sfstderr, "%s(selected) ", n ? n->name : "NONE" );

		for( i = 0; i < f->idx - 1; i++ )
			sfprintf( sfstderr, "%s,", f->nodes[i] );
		sfprintf( sfstderr, "%s(nodes)\n", f->nodes[i] );

	}
}

/* reset all files pointed to by flist to a need state */
void reset_files( File_t **flist, int n, int state )
{
	int i;

	for( i = 0; i < n; i++ )
		if( flist[i] )
			flist[i]->state = state;		/* change state */
}

/* called by symtraverse:
	assign  the file to a job if it is marked as needed
*/
void assign_files( Syment *se, void *data )
{
	File_t	*f;
	Node_t	*n;
	Job_t	*j;
	char	*path;

	if( ! (f = (File_t *) se->value) )
	{
		ng_bleat( 0, "assign_files: se->value was null!" );
		return;
	}

	if( f->state == FS_NEED ) 
	{
		if( ! (n = select_node( f, &path )) )
		{
			ng_bleat( 0, "assign_files: node cannot be selected for: %s", f->name );
			exit( 2 );
		}

		if( ! (j = n->job ) )
		{
			if( ! (j = n->job = mkjob( n )) )
			{
				ng_bleat( 0, "assign_files: unable to create a job: %s(node) %s", n->name, f->name );
				exit( 2 );
			}
		}

		
		add2list( j, f, path );
		f->state = FS_PENDING;
	}
}

/* look up a file and return the block pointer */
File_t *find_file( char *fname )
{
	Syment *se;

	if( (se = symlook( symtab, fname, FILE_SPACE, 0, SYM_NOFLAGS )) )
		return se->value;

	return NULL;
}

/* add a file to the stash, returns 1 if a newly noticed file (for counting) */
int add_file( char *fp, char *np, ng_int64 size )
{
	File_t	*f;
	int	new = 0;
	char	*bp;				/* pointer to the base name (hash name) */

	if( (bp = strrchr( fp, '/' )) )
		bp++;			/* we care only about the basename  for hash reference */
	else
		bp = fp;

	if( (f = find_file( bp )) == NULL )		/* no file in hash table */
	{
		ng_bleat( 3, "add_file: new file: %s", bp );
		new++;
		f = (File_t *) ng_malloc( sizeof( *f ), "add_file: File_t" );
		memset( f, 0, sizeof( *f ) );
		f->name = strdup( bp );
		f->state = FS_NEED;
		f->size = size;
		symlook( symtab, f->name, FILE_SPACE, f, SYM_NOFLAGS );		/* add to table */
		file_sort( f );			/* add file to the external sort list */
	}

	if( f->idx >= f->nnodes )
	{
		f->nnodes += 5;			/* replication factor should be max nodes we should see */
		f->nodes = (char **) ng_realloc( f->nodes, sizeof( char * ) * f->nnodes, "add_file: nodes" );
		f->nattempt = (int *) ng_realloc( f->nattempt, sizeof( int ) * f->nnodes, "add_file: nattemp" );
		f->path = (char **) ng_realloc( f->path, sizeof( char * ) * f->nnodes, "add_file: path" );
	}

	f->nodes[f->idx] = ng_strdup( np );
	f->nattempt[f->idx] = 0;			/* no attempt yet on this node for this file */
	f->path[f->idx] = ng_strdup( fp );				/* save full pathname */
	f->idx++;

	return new;
}

/* read in files from fname 
	returns 0 on failure
	expect nodename filename
*/
int get_flist( char *fname, long expect )
{
	Sfio_t	*f;
	char	*fp;		/* pointer at file name */
	char	*np;		/* pointer at node name */
	char	*sp;		/* pointer at optional size */
	char	*buf;		/* input stuff */
	long	count = 0;
	ng_int64 size;

	if( (f = ng_sfopen( NULL, fname, "r" )) )
	{
		while( (buf = sfgetr( f, '\n', SF_STRING )) != NULL )
		{
			np = strtok( buf, " " ); 		/* point at node name */
			fp = strtok( NULL, " " );		/* point at filename */
			if( (sp = strtok( NULL, " " )) != NULL )
				size = (strtoll( sp, 0, 0 )/size_scale)+1;
			else
				size = 1;

			count += add_file( fp, np, size );		/* add the file to our cache */
		}
	}
	else
	{
		ng_bleat( 0, "could not open file list file: %s: %s", fname, strerror( errno ) );	
		return 0;
	}

	if( expect && (expect != count) )
	{
		ng_bleat( 0, "get_flist: file list count not right: %ld(expected) %ld(read)", expect, count );
		return 0;
	}

	return 1;
}

/* -------- job stuff ------------------------------------------------------------------------- */

/*
	convert a job in old state to new state, others remain unaltered.
	data is pointer to an integer tuple: oldstate newstate
*/
void convert_job( Syment *se, void *data )
{
	Job_t *j;
	int *map;

	map = (int *) data;

	if( (j = (Job_t *) se->value ) != NULL  && j->state == map[0] )
		j->state = map[1];
}

/*
	for each running job we will create a dummy status message and call parse status to 
	fail the job.  se->value expected to be job , *data a pointer to string for status message
	junk section 
*/
void fail_job( Syment *se, void *data )
{
	Job_t	*j;
	char	msg[NG_BUFFER];

	if( (j = (Job_t *) se->value ) != NULL  && j->state == JS_RUNNING )
	{
		sfsprintf( msg, sizeof( msg ), "purge=%s\n", j->name );
		send2seneschal( msg );						/* have seneschal purge it too */
		sfsprintf( msg, sizeof( msg ), "%s:%s:%d:%s", j->name, j->node->name, 126, data ? (char *)data : "forced: no reason" );
		parse_status( msg );
	}
}

/*
	called for each job. if jobs unrep flag is on, then we unreplicate the jobs output file
	we wrap the unreps in a detached wrapper so that we dont delay processing -- unreps can take
	a while depending on how fast the leader responds to rm-where
	also unreps the list file that the job used.
*/
void clean_job( Syment *se, void *data )
{
	Job_t	*j;

	if( (j = (Job_t *) se->value ) == NULL )
		return;					/* should not happen, but be parinoid */
	

	if( keep >= KEEP_INTF  || failed_jobs )
	{
		if( j->phase < 1 )					/* list only files that were phase 0 intermediate files */
			ng_bleat( 0, "intermediate file kept: %s", j->ofname );
		return;							/* keep all intermediate files so leave now */
	}

	if( j->flags & JF_UNREP )			/* we dont unrep the final output file :) */
		unreplicate( j->ofname );		/* will keep the file if its the final output, so safe to call for all jobs */


	if( keep < KEEP_LISTS )				/* if not keeping lists then blow them away too */
		unreplicate( j->lfname );			/* unreplicate the jobs input list */
}

/*	adds the output file from successful job to the list of needed files 
	called by symtraverse for every job
*/
void add_output( Syment *se, void *data )
{
	Job_t *j;
	char *bp;

	if( (j = (Job_t *) se->value ) )
	{
		if( j->state != JS_OK )
			return;				/* assume a job that failed -- dont inlcude its output */

		bp = strrchr( j->ofname, '/' );		/* find basename */
		if( bp )
			bp++;
		else
			bp = j->ofname;

		j->flags |= JF_UNREP;			/* if we add the output of a job to a file list, then we unrep it @end */
		add_file( bp, target_node, 1 );		/* set up to include file in next phase  we don't know size, so everything gets 1*/

		j->state = JS_USED;			/* dont need to use this again */
	}
}

/* look up a job and return the block pointer */
Job_t *find_job( char *name )
{
	Syment *se;

	if( (se = symlook( symtab, name, JOB_SPACE, 0, SYM_NOFLAGS )) )
		return se->value;

	return NULL;
}

void purge_job( Job_t *j )
{
	if( ! j )
		return;

	symdel( symtab, j->name, JOB_SPACE );
	ng_free( j->files );
	ng_free( j->ofname );
	ng_free( j->lfname );
	ng_free( j );
}

/* submit a job -- called by symtraverse */
void submit_job( Syment *se, void *data )
{
	Job_t *j;
	char scmd[NG_BUFFER];		/* buffer to build command string in */
	char *lfbase;			/* basename of the list file */
	char *ofname = NULL;
	int	i;
	ng_int64 size =0;

	if( ! (j = (Job_t *) se->value)  )
	{
		ng_bleat( 0, "submit_job: syment value was null!" );
		return;
	}

	if( j->state != JS_PENDING )
		return;			/* completed or running */

	if( phase )			/* not phase 0, then we write to final output file name */
		ofname = usr_output;
	else
		ofname = j->ofname;	/* name given to the job */
		

	if( j->lf )			/* button up the list -- ready to register it */
	{
		if( sfclose( j->lf ) )
		{
			ng_bleat( 0, "abort: error on write or close; %s: %s", j->lfname, strerror( errno ) );
			exit( 1 );
		}
		
	}

	j->node->job = NULL;		/* this job no longer open to accept things */

	if( j->fidx )			/* there were files given to this job */
	{
		if( j->lf )		/* register the list file, hooded to the node that will run this command */
		{
			/* use wrapper --detach if register blocks too long */
			sfsprintf( scmd, sizeof( scmd ), "ng_rm_register %s -H %s %s", VFLAG, j->node->name, j->lfname );
			ng_bleat( 2, "submit_job: register: %s", scmd );
			system( scmd );
			j->lf = NULL;
		}

		lfbase = strrchr( j->lfname, '/'  ) + 1;	/* we give seneschal just basenames */

		j->state = JS_RUNNING;
		size = j->node->size;		/* just for reporting at end */

		switch( phase )			/* different output files and input list requirements */
		{
			case 0:				
				sfsprintf( scmd, sizeof( scmd ), 
					"job=%s:1:10000:%d:Rcollate %s:%s:0:nomd5,%s:%s:cmd %s\n", j->name, priority, resource, j->node->name, lfbase, j->ofname, usr_cmd );
				ng_bleat( 2, "submit_job: phase %d: %s(name) %s(node) %ld(count) %I*d(sz) %s", phase, j->name, j->node->name, j->node->count, Ii(j->node->size),  scmd );
				send2seneschal( scmd ); 
				j->node->count = 0;
				j->node->size = 0;
				break;

			case 1:						/* send incomplete job then input list */
				sfsprintf( scmd, sizeof( scmd ), 
					"ijob=%s:1:10000:%d:Rcollate %s:%s:0:nomd5,%s:%s:cmd %s\n", j->name, priority, resource, j->node->name, lfbase, *usr_output == '/' ? "" : usr_output, usr_cmd );		/* if user out file fully qualified its in cmd and not as part of job */

				send2seneschal( scmd ); 

				ng_bleat( 2, "submit_job: phase %d: %s(name) %s(node) %ld(count) %I*d(sz) %s", phase, j->name, j->node->name, j->node->count, Ii(j->node->size),  scmd );
				for( i = 0; i < j->fidx; i++ )
				{
					sfsprintf( scmd, sizeof( scmd ), "input=%s:nomd5,%s\n", j->name, j->files[i]->name );
					ng_bleat( 2, "submit_job: %d: %s", phase, scmd );
					send2seneschal( scmd );
				}
				sfsprintf( scmd, sizeof( scmd ), "cjob=%s\n", j->name );
				ng_bleat( 2, "submit_job: %d: %s", phase, scmd );

				send2seneschal( scmd ); 
				j->node->count = 0;
				j->node->size = 0;
		
				break;
		
			default:
				ng_bleat( 0, "submit_job: unknown phase: %s", phase );
				break;
		}

	
		running_jobs++;
		ng_bleat( 1, "submit_job: %s(name) %s(node) %d(files) %I*d(sz)", j->name, j->node->name, j->fidx, Ii(size) );
	}
	else
	{
		ng_bleat( 0, "submit_job: no files for job? %s(name)", j->name );
		
	}
}

/* adds the file to the job, and to the list of files that the job is creating */
void add2list( Job_t *j, File_t *f, char *full_name )
{
	if( j->fidx >= j->nfiles )
	{
		j->nfiles += 200;
		j->files = (File_t **) ng_realloc( j->files, sizeof( File_t * ) * j->nfiles, "add2list" );
	}	

	ng_bleat( 3, "adding to file list: j->name=%s j->fidx=%d fullname=%s", j->name, j->fidx, full_name );

	j->files[j->fidx++] = f;

	if( j->lf )
		sfprintf( j->lf, "%s\n", full_name );		/* add to the list file */
}

/* make a single job */
Job_t *mkjob( Node_t *n )
{
	static int jnum = 0;
	Job_t *j;
	char	buf[1024];

	j = (Job_t *) ng_malloc( sizeof( *j ), "mkjob" );
	memset( j, 0, sizeof( *j ) );

	sfsprintf( j->name, sizeof( j->name ), "coll_%s_%x_%d", n->name, unique_id, jnum );
	symlook( symtab, j->name, JOB_SPACE, j, SYM_NOFLAGS );	

	sfsprintf( buf, sizeof( buf ), "pf-coll-%s-%x-%d.list", n->name, unique_id, jnum );

	j->lfname = ng_spaceman_nm( buf );			/* get a full path in the repmgr area */

	if( (j->lf = ng_sfopen( NULL, j->lfname, "w" )) == NULL )
	{
		ng_log( LOG_ERR, "unable to open list file: %s", j->lfname );
		ng_bleat( 0, "mkjob: could not open list file: %s(job) %s(file): %s", j->name, j->lfname, strerror( errno)  );
		exit( 1 );
	}


	sfsprintf( buf, sizeof( buf ), "pf-coll-%s-%x-%d.out", n->name, unique_id, jnum );
	j->ofname = ng_strdup( buf );


	j->node = n;
	j->state = JS_PENDING;
	j->phase = phase;
	jnum++;
	
	ng_bleat( 2, "mkjob: created: %s(job) %s(node) %s(listfile) %s(outfile)", j->name, j->node->name, j->lfname, j->ofname );
	return j;
}

/* -------------------------------------------------------------------------------------------- */
void usage( )
{
	fprintf(stderr, "Usage: %s [-e expected-count] [-f filelist] [-F final-method] [-P] [-p priority] [-t node] [-v] -o user-outputfile method method-parms\n", argv0);
	exit( 1 );
}

/* we dont expect to see these, but just in case */
void logsignal( int s )
{
	ng_bleat( 4, "ignoring signal received: %d", s );
}


/* expect status message is 
	jobname:node:status:stuff 
*/
void parse_status( char *msg )
{
	Job_t	*j;
	char	*tok;
	int	state;

	tok = strtok( msg, ":" );
	if( ! (j = find_job( tok )) )
	{
		ng_bleat( 0, "parse_status: unknown job on status msg: %s", msg );
		return;
	}

	if( j->state != JS_RUNNING )		/* if we timed it out and it finally completed, we ignore the status */
	{
		ng_bleat( 2, "parse_status: status ignored, job not running: %s(name) %d(state) %s(status)", j->name, j->state, msg );
		return;
	}

	tok = strtok( NULL, ":" );		/* point at node name  -- skip */
	tok = strtok( NULL, ":" );		/* @ status */
	state = atoi( tok );
	tok = strtok( NULL, ":" );

	if( state )				/* zero is good, so !0 is a failure */
	{
		j->state = JS_FAILED;			/* we could trash it, but in case we want stats at the end we keep */
		j->node->state = NS_FAILED;		/* prevent reschedule of job onto this node */
		failed_jobs++;
		ng_bleat( 0, "parse_status: ==> job failed: %s(name) %d(status) %s; %d still running", msg, state, tok, running_jobs - 1 );

		reset_files( j->files, j->fidx, FS_NEED  );		/* reset all files referenced by job to need */
	}
	else
	{
		/* it would be nice to create the need file block now, rather than running through the table 
			when the phase  changes, but if there are errors, we would end up submitting the intermediate 
			files too early. So, later we will run through the jobs, and for the successful ones we 
			will then list their intermediate files.
		*/
		j->state = JS_OK;
		ng_bleat( 1, "parse_status: job done ok: %s; %d still running", msg, running_jobs-1 );
		reset_files( j->files, j->fidx, FS_HAVE  );		/* all files referenced here are ok */

	}
	
	running_jobs--;			
}

/* 
	called with each timer pop. checks to see if all jobs have finished, and if so either 
	builds new ones to cover for failures, or starts the second wave of jobs to roll together
	the output from the first wave.

	returns 1 if we are done and alarm callback should exit
*/
int job_maint(  )
{
	static	int fmsg = 0;			/* we issue the failed message only after the initial submission */
	static  int next_bleat = 10;
	int	rc = 0;				/* return code - set to 1 if we need to exit */

	if( expiry && expiry < ng_now( ) )		/* ut-oh, timeout jobs that are still running */
	{
		ng_bleat( 1, "maximum wait time exceeded (%lds); timing out remaining jobs", timeout );
		symtraverse( symtab, JOB_SPACE, fail_job, "timeout" );
		expiry = 0;
	}

	switch( phase )
	{
		case 0:						/* first wave of jobs still pending */
			if( ! running_jobs )			/* all jobs finished? */
			{
				if( failed_jobs )		/* if failures, try again (set for the first pass to kick the initial submission) */
				{
					if( fmsg++ )		/* dont write first time through */
						ng_bleat( 1, "job_maint: some jobs failed, creating job(s) to collate files from those jobs" );
					failed_jobs = 0;
#ifdef ORIGINAL
					symtraverse( symtab, FILE_SPACE, assign_files, NULL );		/* assign files to jobs */
#endif
					file_sort( NULL );					/* sort the files and do the assignment */
					symtraverse( symtab, JOB_SPACE, submit_job, NULL );		/* submit pending jobs */
					if( timeout )
						expiry = ng_now( ) + (timeout * 10);	/* timeout is seconds, expiry is tenths */
				}
				else			/* we graduate to phase 1 */
				{
					ng_bleat( 1, "job_maint: all intermediate jobs complete, creating final collation job" );
					usr_cmd = final_cmd;		/* switch to final rollup command */

					phase = 1;

					/* order important: make fileblocks for each successful phase 0 job,
						run the file list to generate phase 1 job (should be just 1)
						submit the job for phase 1 
					*/
					symtraverse( symtab, JOB_SPACE, add_output, NULL );	
					symtraverse( symtab, FILE_SPACE, assign_files, NULL );
					symtraverse( symtab, JOB_SPACE, submit_job,  NULL );
				}
			}
			else
				if( --next_bleat < 1 )
				{
					ng_bleat( 3, "job_maint: phase %d: %d jobs still running", phase, running_jobs );
					next_bleat = 20;			/* about 1 min between */
				}
			break;


		case 1:			/* phase 1 -- just waiting for the second rollup to finish */
			if( ! running_jobs )
			{
				ng_bleat( 2, "job_maint: phase 1 completed. " );
				rc = 1;			/* cause si to return control to main; main's exit code will be faiures :) */
			}

			break;

		default:
			ng_bleat( 0, "job_maint: unknown phase: %d; abort", phase );
			abort( );
			break;
	}

	return rc;
}

/* ----------------------------------------------------------------------------------------------------------- */

/*
	collects contents of files from the input list using the command (e.g. cat) supplied on the 
	commandline. A job is started on each node (with seneschal) that generates a rolled-up file
	and registers that file with a hint to go to a common node.  When all of the first wave roll 
	jobs are done a final job is started that rolls the firstwave output into the final file. 
*/
int main(int argc, char **argv)
{
	char	*port = NULL;
	char	*cp;				/* general chr pointer */
	char	wbuf[256];			/* work buffer */
	char	*flist = "/dev/fd/0";		/* file where we expect to read the file list from */
	int	expect = 0;			/* # of unique basenames expected in list -- if 0 we dont check */

	signal( SIGPIPE, logsignal );		/* we dont care about broken pipe messages */

	port = "0";				/* rightnow we open any random port */

	ng_sysname( wbuf, sizeof( wbuf ) );
	if( (cp = strchr( wbuf, '.' )) != NULL )
		*cp = 0;				/* make ningxx.research.att.com just ningxx */
	mynode = ng_strdup( wbuf );

	target_node = mynode;

	ARGBEGIN
	{
		case 'a':	IARG( attempts ); break;	/* number of attempts to give to seneschal -- default is 1*/
		case 'e':	IARG( expect ); break;
		case 'f':	SARG( flist ); break;
		case 'F':	SARG( final_method ); break;	/* method to run against all files produced during phase 0 */
		case 'k':	OPT_INC( keep ); break;		/* -k keep lists, -k -k keep lists and intermediate files */
		case 'o':	SARG( usr_output ); break;
		case 'p':	IARG( priority ); break;
		case 'P':	passthru++; break;
		case 'r':	SARG( resource ); break;			/* resource in addition to Rcollate */
		case 's':	assign_by_count = 0; break;			/* assign jobs based on file size */
		case 'S':	assign_by_count = 0;  sort_by_size= 0; break;	/* files assigned by size, but not sorted */
		case 't':	SARG( target_node ); break;			/* here by default, user override */
		case 'T':	IARG( timeout ); break;
		case 'v':	OPT_INC( verbose ); break;
		default:
usage:
			ng_bleat( 0, "unrecognised: %s", argv[0] );

			usage( );
			exit(1);
	}
	ARGEND

	ng_bleat( 0, "%s: v1.5/03258: starting! pid=%d", argv0, getpid() );  /* version */

	if( (cp = ng_env( "NG_RM_FS_FULL" )) != NULL )		/* user defined min repmgr filesystem size for output files */
	{
		rm_fs_full = atoi( cp );			/* save it */
		ng_bleat( 1, "NG_RM_FS_FULL=%d  will be passed to all rollup processes", rm_fs_full );
	}

	if( !usr_output )
	{
		usage( );
		exit( 2 );
	}

	if( priority > 95 )
	{
		ng_bleat( 0, "priority was set too high; reduced to 95" );
		priority = 95;
	}


	usr_cmd = build_cmd( argc, argv, NULL, NULL );
	final_cmd = build_cmd( 0, NULL, final_method, *usr_output == '/' ? usr_output : NULL );

	unique_id = (ng_now( )<<16) + getpid( );		/* for now this will work */
	symtab = syminit( 49999 );

	if( ! get_flist( flist, expect ) )
		exit( 1 );

	port_listener( port );					/* open network connections, and run */

	symtraverse( symtab, JOB_SPACE, clean_job, NULL );	/* unregister intermediate files */

	ng_bleat( failed_jobs ? 0 : 1, "main: processing finished, final status is: %d", failed_jobs );

	exit( failed_jobs );

	return 0;				/* keep compilers happy */
}

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_collate:Cluster wide file collation manager)

&space
&synop	ng_collate [-e file-count] [-f filelist] [-F final-method] [-o usr-output] [-p priority] [-t node] [-T seconds] [-v] [--man] -o outputfile method-command

&space
&desc	&This manages the combination of files from round the cluster causing them to 
	be collated into a single file.  Collation occurrs by submitting a job to each 
	node which 'rolls-up' the files on that node into a single, intermediate, file. 
	Each intermediate file
	is registered with the replication manager and a final job is submitted to 
	combine the intermediate files into a single result file. 

&subcat	The File List
	&This will collect only the files that are supplied via the file list (using either the 
	&lit(-f) option, or piping the list on the standard input device).  
	Each record in the file list is expected to describe a file giving as the first token the 
	node name, followed by the fully qualified pathname of the file on that node.  When using the 
	&lit(-s) option the file size must be supplied as the third token on the record. 
	Spaces should separate the two tokens.  It is expected that the same file basename will be encountered 
	multiple times in the list, once for each node that the file exists on (this is a good thing as it 
	allows &this to recover if a node fails).

&subcat The Collation Method
	The method that is used to combine files can be any programme that accepts the names of files
	on the command line, and writes the resulting single file to the standard output device. 
	The &lit(ng_rollup) script is invoked by &this to actually coordinate the combination of 
	each file on a single node, and to combine each of the intermediate files into the 
	final results file. The &lit(ng_rollup) script assumes that the method programme can 
	be invoked with &lit(xargs).  


&subcat Job Management
	Two phases of jobs are submitted directly to &lit(ng_seneschal). During the first phase, 
	jobs are submitted, one per node containing files that need to be collated, to create 
	the intermediate files.  Should a phase one job fail, a second set of jobs are submitted
	which attempt to create intermediate files containing the contents of the files that 
	were originally being combined in the failed job. 
&space
	Once all intermediate files have been produced, the second phase is entered and a single 
	job is submitted that will combine all of the intermdeiate files.  Upon success of the 
	second phase, all intermediate files are unregistered, and the final output file is 
	registered with the replication manager. Should the second phase fail, no attempt is 
	made to resubmit the job and &this exits with a bad return code. 

&space
	If the session to &lit(ng_seneschal) is lost, all running jobs are considered to be pending 
	and are resubmitted to &lit(ng_seneschal) when a connection is reestablished.  If a timeout 
	value is supplied, the timer is reset when the connection is reestablished.



&opts	The following options are recognised by &this when placed on the command line:
&begterms
&space
&term	-e count : The number of files that &this should expect to find in the file list. If this 
	number is not found, then &this will exit with an error.
&space
&term	-f file : Supplies the name of a file containing the file list. The file list is a complete
	list of all instances of the files that are to be combined. It is expected that the list 
	will contain duplicate basenames that have multiple occurances in replication manager.
	The files must be listed one per record with each record containing the node name 
	followed by the fully quallified pathname of the file on that node.  If this option is not 
	supplied, &this expects to read the list of node, path, size tripples from the standard input device. 
	The size is optional, but must be supplied if the -s option is used. 
&space
&term	-F method : Defines a method to be run as the final rollup method.  This method is applied 
	to all of the files generated with the generic method (supplied as command line parameters).
	If this option is not supplied, the generic method is run against the intermediate files.
&space
&term	-k : Keep lists and/or intermedate files. If one -k is placed on the command line, then the 
	file lists supplied to each rollup job are kept in replication manager space. If two -k options
	are supplied (or -kk) then the intermediate files are kept in addition to the rollup lists.
	When this option is not supplied the intermediate files are unregistered, and removed from 
	replication manager space.
&space
&term	-o file : Defines the name of the file that is generated as the final result file. This file 
	will be registered with the replication manager.  This is required.
&space
&term	-P : Passthrough.  The rollup jobs are invoked with -P option to pass through the file list.
&space
&term	-p n : Sets the job priority for seneschal jobs. Default is 75, max that can be set is 95.
&space
&term	-s : Divvy by size.  Jobs are assigned files based on the size of the files.  Files listed
	must have node, filename, size tripples to use this option.  If not supplied, jobs are assigned
	files based on count rather than size. 
&space
&term	-t node : Specifies the name of the node where the intermediate files should be migrated 
	for the final phase two job. If this option is not specified, the node on which &this is 
	executing is used. 
&space
&item	-T sec : Allows a timeout value to be supplied. Jobs are considered to have failed if they 
	have not completed before the number of seconds indicated have passed. If no timeout value 
	is supplied, &this will wait patiently until a status on all jobs has been returned. Seneshcal 
	will only attempt to run each job as bids, and required files, become available 
	on the necessary node, and as &this shuffles list files between nodes, any replication manager 
	latency could make guessing a reasonable timeout difficult.
&space

&term	-v : Verbose mode. Several can be placed on the command line to increase the noise
	created by &this. A single verbose flag should cause &this to generate only messages realated
	to collation progress; multiple &lit(-v) flags are needed for debugging help.
&endterms

&space
&parms	Following any necessary options, &this expects that all remaining command line parameters 
	are the method command and arguments that are to be used to collect the files. 

&space
&files
	&This creates a list file for each job that is exectued. These list files are named using 
	the syntax: &lit(pf-coll-<node>-<id>-<n>.list). These files contain the list of files that 
	each job is responsible for collating, and are passed to the appropriate node for the job 
	via the replication manager. 
&space
	The intermediate files, created by each of the first phase jobs, are also registered with the 
	replication manager, and allowed to migrate to the target host via that mechanism.  After 
	completion of the second phase job, these files, and the list files, are unregistered and allowed
	to be purged. 

&space
&exit	
	An exit code of zero (0) indicates that the requested output file was created without any errors.
	The file will be registered with replication manager. If an non-zero return code is emitted, then
	there were errors, and the final file is not registered.
&space
	A common error message (node cannot be selected for: <name>) indicates that &this is unable to 
	create a job combination that results in the named file being included.  This error generally 
	happens when &this is trying to build a second set of jobs to recover form a node/job failure. 
	Usually, but not always, this error indicates that the named file existed only once on the cluster
	or was included in the file list just once. 

&space
&see	ng_rollup, ng_nawab, ng_seneschal

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 	22 Apr 2003 (sd) : First code.
&term	22 Aug 2003 (sd) : Modified to accept a secondary method to be run against the 
	intermediate files produced during phase 0 processing.
&term	22 Jan 2004 (sd) : Changed the job string to be job= rather than the deprecated default of no label.
&term	10 Feb 2004 (sd) : Added passing of -j jobname to rollup call, and to increase the attempts to the 
	standard 3.
&term	12 Feb 2004 (sd) : Added alarm call after message receipt to avoid wedging because of missed signals.
&term	09 Mar 2005 (sd) : Revampped assignment algorithm to better distribute files (more evenly). (v1.2)
&term	10 Jan 2006 (sd) : added required input field to job strings sent to seneschal. (v1.3)
&term	24 Aug 2006 (sd) : Now examines the NG_RM_FS_FULL value and causes it to be passed into the 
		environment of each rollup command invoked by seneschal. (v1.4)
&term	25 Mar 2007 (sd) : Added -s option to allow file list to be created based on size rather than 
		counts. (v1.5)
&term	09 Jun 2009 (sd) : Remved reference to deprecated ng_ext.h header file.
&endterms

&scd_end
------------------------------------------------------------------ */
