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

#ifndef _ng_lib_h 
#define _ng_lib_h
/*
	Modified: 	29 Sep 2004 (sd) : Added ng_read prototype
			31 Jul 2005 (sd) : Added protos as grumbled about by gcc -Wall flag
			16 Nov 2006 (sd) : added thread safe global variable support for sfdc_errno
			26 Sep 2008 (sd) : Added prot for ng_dpt_set_noroot() 
*/

#include	<syslog.h>
#include	<sys/socket.h>
#include	<netinet/in.h>

#include	"ng_types.h"

typedef int bool;
#undef FALSE
#undef TRUE
enum { FALSE, TRUE };

#define UNIX_OFFSET	1186897129	/* time(0) = ng_now()/10 + UNIX_OFFSET -- needed in ng_time */

#define ng_cmp(a,b) ((a)>(b)?1:(a)<(b)?-1:0)	/* compare a/b return 0 if ==, 1 if a>b and -1 if a<b */

#define STATS_START		1           /* action codes for ng_stats calls */
#define STATS_END		2           /* mark end, dont write */
#define STATS_WRITE		3           /* end if needed, write to log */
#define STATS_STDERR		4           /* write, then echo to stderr */


typedef ng_uint64	ng_hrtime;	/* for high resolution time */
extern double		hrtime_scale;	/* seconds = (hrt1-hrt2)*hrtime_scale */
extern ng_hrtime	ng_hrtimer(void);


typedef struct
{
	FILE	*rec_fp;
	int	rec_size;
	int	force_var;
	int	rdw_len;
	int	ret_rdw;
	int	incl_rdw;
	int	len;
	int	len2;
	ng_byte	rec_buf[256*1024];
	ng_byte	*next;
	ng_byte	*last;
	ng_byte	*last_rec;
	ng_int64	rseek;		/* offset of rec_buf[0] */
} Record_file;

/*
   fnameparse function (which) constants. order is EXACTLY the same as field order,
   sequential numbers are zero based from the FMP_XX_XX constant.
*/
#define FNP_DS_DB       0       /* get the database name */
#define FNP_DS_PART     1       /* get the partition id */
#define FNP_DS_EDITION  2       /* get the edition number */
#define FNP_DS_ATTEMPT  3       /* get the attempt component */
#define FNP_DS_SUFFIX   4       /* get the suffix */

#define FNP_FD_STREAM   50      /* get the stream name */
#define FNP_FD_FILEN    51      /* get the filename */
#define FNP_FD_SUFFIX   52      /* get the suffix from the name */

#define FNP_PF_DB       100     /* get the database name */
#define FNP_PF_EDITION  101     /* get the edition string */
#define FNP_PF_STUFF    102     /* get the stuff */
#define FNP_PF_SUFFIX   103     /* get the suffix */

/*
   These flags are used in vetter for checking the record counts.
*/

#define	RECCNT_NO_APPLICABLE	0       /* no rec cnt in hdr and tlr */
#define	RECCNT_INCLUDE_HT	1       /* rec count includes hdr and tlr */
#define	RECCNT_NOT_INCLUDE_HT	2	/* rec count does not include hdr and tlr */



/* --------- misc prototypes ---------------------- */

extern char	*ng_getdsname( int, char *, char *, int );
extern char	*ng_getrdsname( int, char *, char *, int );  		/* row oriented dsname */
extern	char	*ng_cat_dsn2name( int dsn );				/* convert dsn to catalogue name */
extern int	ng_cat_name2dsn( char *name );				/* convert name into a dsn */
extern void	ng_cat_close( );					/* close tcp session to catalogue */

extern void	*ng_malloc(int nng_bytes, char *errmsg);
extern void	ng_free(void *storage);
extern void	*ng_realloc(void *storage, int nng_bytes, char *errmsg);
extern Record_file	*ng_rec_init(FILE *fp, int rtype, int rsize);
extern ng_byte	*ng_rec_get(Record_file *rf);
extern ng_int64	ng_rec_offset(Record_file *rf, ng_int64 *cur);
extern char	*ng_strdup(char *);

extern void 	ng_die( void );
extern int	ng_atdeath( void f(), void *data );


extern char     *ng_getname( int p, char *basename, char *buf );
extern void	ng_sleep( time_t secs );		/* thread safe sleep */
extern char	*ng_leader( );				/* return leader name */
extern char	*ng_rootnm( char *s );			/* return NG_ROOT appended to /s */

extern ng_int64	ng_string_to_int64(ng_byte *, int);
extern int	ng_string_to_int(ng_byte *, int);

extern int 	ng_name_dsn(char *);
extern char	*ng_dsn_name(int);
extern char	*ng_env( char * );
extern const char	*ng_env_c( char * );
extern char	*ng_eparam( char * );

extern void	etoa(register ng_byte *, register ng_byte *, int);
extern ng_byte	ea_tab[256];
extern ng_byte	ae_tab[256];
extern ng_int64	etoi(ng_byte *, int);
extern ng_int64	btoi(ng_byte *, int);
extern ng_int64	bcdtoi(ng_byte *, int);
extern int	bcd(ng_byte *, int);
extern int	bcdl(ng_byte *, int);
extern int	bcdlim(ng_byte *, int, int);
extern double	gbcd(ng_byte *, int);
extern void	bcdasc(ng_byte *, int, ng_byte *);
extern void	ascbcd(ng_byte *, int, ng_byte *);
extern void 	shiftlbcd(ng_byte *src, int n, ng_byte *dest);
extern void	intbcd(int, ng_byte *, int);
extern ng_uint64	e10(int n);
extern void	ng_srand(void);
extern ng_uint32	ng_sum32(ng_uint32 lcrc, ng_byte *buf, int n);
extern int	ng_tzoff(char *tz, long t);

/* --------- symbol table things ------------------- */
typedef struct Syment
{
	int		space;
	int		flags;
	char		*name;
	void		*value;
	struct Syment	*next;
} Syment;

typedef struct
{
	int		nhash;
	Syment		**syms;
} Symtab;

#define SYM_NOFLAGS	0x00		
#define SYM_COPYNM	0x01		/* symtab should make a private copy of the name */
#define SYM_USR1	0x4000		/* reserved for user's use */
#define SYM_USR2	0x8000

extern Symtab	*syminit(int nhash);
extern Syment	*symlook(Symtab *st, char *name, int space, void *install, int flags );
extern void	symdel(Symtab *st, char *sym, int space);
extern void	symtraverse(Symtab *st, int space, void (*fn)(Syment *, void *), void *arg);
extern void	symstat(Symtab *st, Sfio_t *);
extern void	symclear( Symtab *st );			/* clear ALL symtab entreis; does NOT free user pointers */


/* -------------- logging and bleeting ----------------------------------- */
					/* catigory masks for cbleat */
#define CBLEAT_ALL	0x00		/* bleat for all catigories, even none */
#define CBLEAT_LIB	0x01		/* library functions should use this */
#define CBLEAT_NET	0x02		/* network oriented library functions */
#define CBLEAT_USR   	0x10		/* general user app */
#define CBLEAT_USRLIB	0x20		/* user app library */
#define CBLEAT_USRNET	0x40		/* user app network */

extern void     ng_bleat( int vlevel, char *fmt, ...);
extern void     ng_cbleat( int catigory, int vlevel, char *fmt, ...);

extern void     ng_log_reinit( );				/* resets the log stuff - needed after fork */
extern void     ng_log( int priority, char *fmt, ...);
extern void     ng_alog(int priority, char *logname, char *id, char *fmt, ...);
extern int	ng_log_bcast( char *buf, int len, int seq );	/* if len is 0 then buf is assumed to be null terminated */
extern void	ng_stats( int action, ...);

/* -------- ng_serv and ng_rx ------------  */
extern int	ng_serv_announce(char *service);
extern int	ng_serv_probe(int sock);
extern void	ng_serv_unannounce(int sock);
extern int	ng_rx(char *mach, char *service, int *ifd, int *ofd);
extern int	ng_port_map(char *service);

/* ------- ng_perror --------------------- */
extern const char	*ng_perrorstr(void);
extern void	perror(const char *s);

/* ------------ npa ------------------- */
extern int ng_init_sif_db(char *path_to_db, int split_method);
extern ng_int64	ng_npasplit_wtn(ng_int64 wtn, ng_timetype date, int split_method);

/* --------- vdsqueeze ------------------ */
extern	int	vdsqueeze(void* src, int slen, void* dest);
extern	int	vdexpand(void* dest, int dlen, void* src);

/* --------- fdump --------------- */
extern	void	fdump(Sfio_t *f, unsigned char *data, int len, int flags, char *s); 
#define D_OFFSET	0x00		/* show offset */
#define D_EBCDIC	0x01		/* convert cheater to EBCDIC */
#define D_ADDR		0x02		/* show address on left */
#define D_ROFF		0x04		/* running offset for things like file dumpers */
#define D_DECIMAL	0x08		/* show offset in decimal */
#define D_ALL		0x10		/* dont snip if repeated things */


/* -------- fnameparse --------------- */
char *ng_fnameparse( char *fname, int what );	/* parses a standard ningaui file name returning a piece of it */
char *ng_fnamecmpnt(char *fname, int which);  /* parse fname into components */
#define FNC_BASENAME	0	 /* define of parameter "which" for ng_fnamecmpnt function */
#define FNC_GENVER	1
#define FNC_DATE	2
#define FNC_SUFFIX	3
#define	FNC_VALBASENAME	4


/* ---------- TMPS combo component type ---------- */

#define	COMBO_SUR	10
#define COMBO_EMC	11
#define COMBO_IHWSW501	12
#define COMBO_IHWSW001	13
#define COMBO_IHWSW051	14

/* ---------- ng_netif ---------- */
extern	char	*ng_addr2dot( struct sockaddr_in *src );				/* convert sockaddr to string */
extern	char	*ng_ip2name( char *ip_string );						/* convert ipv4 to a hostname */
extern	struct	sockaddr_in *ng_dot2addr( char *src );  				/* convert string addr to sockaddr struct */
extern	int	ng_openudp( int port );                    				/* open a udp port */
extern	int	ng_open_netudp_r( char *net, int port, int reuse_port );		/* open a udp port with optional resuse flag */
extern	void	ng_closeudp( int fd );                    				/* close a udp port */
extern	int	ng_writeudp( int fd, char *to, char *buf, int len );  			/* write buffer to udp addr in string */
extern	int	ng_readudp( int fd, char *buf, int len, char *from, int delay );  	/* read buffer from udp */
extern	int	ng_write_a_udp( int fd, struct sockaddr_in *to, char *buf, int len );	/* send udp message to addr in the addr struct */
extern	char	*ng_getbroadcast( int fd, char *ifname );  	 			/* get the broadcast address for an interface */
extern	int 	ng_mcast_join( int fd, char *group, ng_uint32 iface, unsigned char ttl );	/* join a mcast group */
extern	int	ng_mcast_leave( int fd, char *group, ng_uint32 iface ); 		/* leave a group */

	/* care must be taken when using these.  microsecond delays less than 50,000 seem ineffective */ 
extern	int	ng_testport_m( int fd, int delay, int mdelay );		/* test for read ready on fd, delay up to delay.mdelay sec */
extern	int	ng_testport( int fd, int delay );			/* delay of 0, results in a real .05s delay */

/* ---------- checkpoint stuff ---------- */
extern	void ng_ckpt_seed( char *seed, int max_tumbler_low, int max_tumbler_hi );
extern	char *ng_ckpt_name( char *basename, int managed );
extern	char *ng_ckpt_peek( char *basename, int managed );

/* ---------- tumbler stuff ---------- */
typedef struct {
	int	hi_now, hi_max;		/* high frequency tumbler */
	int	lo_now, lo_max;		/* high frequency tumbler */
	int	scope;
	char	*nvarname;
	char	*pre, *suf;
} Tumbler;
extern	Tumbler *ng_tumbler_init(char *nvar);
extern char *ng_tumbler(Tumbler *t, int managed, int peek, int peek_offset);

/* ---------- spaceman things----------  */
extern char *ng_spaceman_nm( char *base );
extern char **ng_spaceman_bunch( int n );		/* return a bunch of paths in the file arena */
extern char *ng_spaceman_local( char *fsname, long hldcount, char *fname );
extern char *ng_spaceman_rand( char *list, char *fname );
extern void ng_spaceman_reset( );
extern int ng_spaceman_hldcount( char *fsname );
extern void ng_spaceman_fslist( char *list, int list_len );

/*  ----------- ng_fish stuff: fixed record length binary file/buffer search in ng_fish.c -------------- */
extern char *ng_fish_file( int fd, void *target, int klen, long reclen, long count, long key_off );
extern char *ng_fish_buf( char *buf, void *target, int klen, long reclen, long nrec, long key_off );

/* ------------ ng_scramble/unscramble ----------------------- */
extern unsigned char *ng_scramble( unsigned char *k, unsigned char *s );		/* scramble an ascii string */
extern unsigned char *ng_unscramble( unsigned char *k, unsigned char *s );		/* unscramble an ascii string that was scrambled using ng_scramble */

/* ----------- tokeniser ----------- */
char **ng_tokenise( char *b, char sep, char esc, char **reuse, int *count );
char **ng_readtokens( Sfio_t *f, char sep, char esc, char **reuse, int *count );


/* ---------- sysctl things  ---------- */
extern long ng_memsize( );			/* number of bytes of memory */
extern int ng_byteorder( );			/* byte order indication */
extern double *ng_loadavg( );			/* return array of three load ave values */
extern double *ng_cp_time_pctg( );		/* return cpu time as percentages */
extern long *ng_cp_time_delta( );		/* return cpu time as delta ticks */
extern int ng_ncpu();				/* number of cpus on node */

/* ---------------  flow: stream buffer flow management (socket) ------------- */
void *ng_flow_open( int size );  
void ng_flow_close( void *handle );
void ng_flow_flush( void *handle );
void ng_flow_ref( void *handle, char *buf, long len );
char *ng_flow_get(void * handle, char sep );	

/* ----- misc stuff ----- */
extern char 	*ng_cmd( char *cmd );		/* run a command, return output in a null terminated buffer */
extern char 	*ng_bquote( char *buf );		/* expand a string that has `cmd` embedded somewhere */
extern ng_uint32  ng_cksum(ng_uint32 prev_crc, ng_byte *buf, int n);
extern int 	ng_read( int fd, char *buf, size_t len, int timeout );	/* read with a max wait timeout */
extern void 	show_man( );
extern void 	ng_open012( );						/* ensure fds 0/1/2 are open -- pesky bug in ksh if they are not */
extern char 	*ng_tmp_nm( char *id_string );		/* generate a temp file name, create the file in $TMP/<id_string>.pid.<unique_string> */
extern int 	ng_valid_ruser( char *runame, char *rhost );				/* return true if user is considered legit */
extern unsigned char *ng_scramble( unsigned char *key,  unsigned char *buf );		/* scramble a buffer given a key */
extern unsigned char *ng_unscramble( unsigned char *key,  unsigned char *sbuf );	/* unscramble a scrambled buffer using key */
extern int	ng_sysname(char *buf, int size);

/*#include <ng_ext.h> ext.h is now deprecated!!!! */			/* dont know why it exists; likely leftovers from gecko */


/* -------- narbalek and pinkpages intrface - ng_nar_if.c --------------- */

#define PP_FLOCK	0			/* scope of pinkpages variables in narbalek */
#define PP_CLUSTER	1
#define PP_NODE		2
#define PP_NCF_ORDER	4			/* search for variable in namespace using node/cluster/flock order */

extern int ng_nar_open( const char *port );		/* open a socket to narbalek (automatic on first set/get) */
extern void ng_nar_close( );				/* close the socket */
extern char *ng_nar_get( char *vname );			/* get value of variable vname (if vname is a leased var, lease info will remain as part of value) */
extern char *ng_nar_get_lv( char *vname );		/* get a leased value from variable vname; leased info is removed from the value */
extern int ng_nar_set( char *vname, char *value );	/* set value of variable  */
extern int ng_nar_set_lv( char *vname, char *value, int seconds, int fast );	/* set a leased value of variable, lease expires seconds from current time  */
extern void ng_nar_retries( int s, int g );		/* change set and get retry values; default is 2 each, values may be >= 0 */

extern void *ng_nar_cache_new( char *pattern, int refresh );	/* create a new cache and return pointer to internal struct */
extern int  ng_nar_cache_refresh( void *cache );		/* load the cache and return 1 if successful, 0 on error */
extern void ng_nar_cache_close( void *cache );			/* trash the cache */
extern char *ng_nar_cache_get( void *cache, char *vname );	/* get a value based on var name from cache */
extern void ng_nar_cache_setsize( int size );			/* set the size of the cache list array -- default is 4k */

extern char *ng_pp_get( char *vname, int *where );		/* get from the pinkpages namespace - search node/cluster/flock namespaces */
extern char *ng_pp_scoped_get( char *vname, int scope, int *where ); /* get from pinkpages with a forced scope */
extern char *ng_pp_node_get( char *vname, char *node ); 		/* get value as it applies to the named node */
extern int ng_pp_set( char *vname, char *value, int scope );
extern char *ng_pp_expand( char *sbuf );			/* expand a string with embedded ppinkpages $vname references */
extern char *ng_pp_expand_x( char *sbuf, char vsym, char esym, int vex_flags );		/* expand $ppnames thta are in sbuf with more control */

/* ------------ variable expansion ng_var_exp() and ng_pp_expand() ---------------------------------- */
#define VEX_CONSTBUF 	0		/* dummy flag  - default is to use the static buffer */
#define VEX_NEWBUF	0x01		/* create a new buffer */
#define VEX_NORECURSE	0x02		/* no recursive expansion */
#define VEX_CACHE	0x04		/* cache pinkpages data (pp_expand only) */

char *ng_var_exp( Symtab *st, int namespace, char *sbuf, char vsym, char esym, char psym, int flags );


/* ------------- sfio interface but only if we've included sfio.h first (allow some programmes to avoid sfio) --------------- */
#ifdef _SFIO_H
extern Sfio_t *ng_sfopen( Sfio_t *f, char *name, char *mode );
extern Sfio_t *ng_sfpopen( Sfio_t *f, char *name, char *mode );
#endif


/* ------------ ng_time things ---------------- */

typedef struct
{
	double		usr;
	double		sys;
	double		real;
} Time;
extern	void	ng_time(Time *);

extern  char    *argv0;
extern  ng_timetype	ctimeof(int hhmmsss);
extern  ng_timetype	etimeof(int mmmmmsss);
extern  ng_timetype	cal2jul(long ymmdd);
extern  void    	julcal(long jul, int *year, int *month, int *day);
extern  ng_timetype	ng_timestamp(char *);
extern	ng_timetype 	ng_utimestamp( char *buf );
extern	ng_timetype 	ng_utimestamp_gmt( char *buf );
extern  char   		*ng_stamptime(ng_timetype, int, char *);
extern 	ng_timetype 	dt_ts(long, long);
extern  char		*ng_getdts(void);
extern  char		*ng_gettimes(void);
extern  char		*ng_getdates(void);
extern	ng_timetype	ng_now(void);

/* -------- md5 interface --------------- */

typedef struct
{
	ng_uint32	state[4];	/* state (ABCD)			*/
	ng_uint32	count[2];	/* # bits handled mod 2^64 (lsb)*/
	unsigned char	buffer[64];	/* input buffer			*/
	unsigned char	digest[16];	/* final digest			*/
	unsigned char	digest_sum[16]; /* sum of all digests		*/
} Md5_t;

extern Md5_t *md5_init(void);
extern void md5_block(Md5_t* x, const void *s, size_t slen);
extern void md5_fin(Md5_t *x, char *result);

/*
	md5_init returns a (ng_malloced) initialised structure.
	call md5_block repeatedly for all the data to be checksummed.
	md5_fin finalises the checksum, dumps it in result, and then ng_frees the Md5_t structure.
	the result buffer needs to be at least 33 bytes long.

	here is a sample routine to checksum one file:

	void
	do1(FILE *fp, char *result)
	{
		Md5_t *m5;
		char buf[NG_BUFFER];
		int k;
	
		m5 = md5_init();
		while((k = fread(buf, 1, sizeof(buf), fp)) > 0){
			md5_block(m5, buf, k);
		}
		md5_fin(m5, result);
	}
*/

/*
 -------  dptree (distributed process tree) routines (ng_dptree.c) ------------
*/
#ifndef _dptree_h
#define _dptree_h
						/* values that flags can have when dpt_user_maint() is invoked */
#define	DPT_FL_ROOT		0x01		/* we are the root of all evil */
#define DPT_FL_ADOPTED		0x02		/* we have a parent */
#define DPT_FL_DELAY_AR		0x04		/* delay the adoption request */
#define DPT_FL_ROOT_KNOWN	0x08		/* we've noticed another node has declared that they are root */
#define DPT_FL_UNUSED1		0x10		/* 	available */
#define DPT_FL_UNUSED2		0x20		/* 	available */
#define DPT_FL_NO_ROOT		0x40		/* we are not allowed to become the root */
#define DPT_FL_NO_MCAST		0x80		/* we are isolated and dont have multi cast -- use parrot to agent us into the group */
#define DPT_FL_UNUSED3 		0x100		/* 	available */
#define DPT_FL_HAVE_LIST	0x200		/* we have a peer list, though it is legit to be null */
#define DPT_FL_USE_NNSD		0x400		/* use the nnsd process for lookup rather than multicast */
#define DPT_FL_HAVE_AGENT 	0x1000		/* needed positional for nar_map to indicate via agent */
#define DPT_FL_QUERY_PENDING 	0x2000		/* weve sent a query to our peers to see if there is a better suited parent */
#define DPT_FL_COLLISION	0x4000		/* root collision since last call -- if FL_ROOT is set this process won, else it lost */

/*
        --------------- prototypes of user functions dptree functions invoke ------------------- 
	user functions exit 0 to force a shutdown; any non-zero exit is considered good
	users must supply these two funcitons when if the programme invokes ng_dptree()
*/
extern int dpt_user_data( int sid, char *rec, int flags );	/* driven for each record received on a TCP/IP session */
extern int dpt_user_maint( int flags );				/* driven on a regular basis for the user to do work */

extern int ng_dptree( char *league, char *cluster, char *port_str, char *mcast_agent, char *mcast_group, char *nnsd_port_str );
extern void ng_dpt_propigate( int sid, char *buf );		/* send the message to all partners that are not sid (make sid -1 to send to all) */
extern void ng_dpt_set_ttl( int n );				/* set time to live hops for mcast messages */
extern void ng_dpt_set_max_depth( int n );			/* set the max tree depth; dpt aborts if it appears depth is > than n, n > 10 */
extern void ng_dpt_set_noroot( int state );			/* allow user to set/clear no_root flag */
extern void ng_dpt_set_tflag( unsigned int f );			/* allow user to set the tree command character */
#endif


/* ------------------------------------------- */

#endif
