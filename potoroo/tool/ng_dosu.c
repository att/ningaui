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
 ---------------------------------------------------------------------------------
 Mnemonic:	ng_dosu
 Abstract:	this is a SETUID ROOT programme that we use to execute a very 
		limited number of things.  It is NOT like sudo that allows 
		for explicit command execution even in a limite fashion. 

		This is compiled into ng_FIXUP_dosu so that when we install with
		a refurb it does not automatically overlay the suid-root copy and 
		revert permissions to ningaui.  The system manager should copy 
		ng_FIXUP_dosu to ng_dosu and set the permissions only when the 
		version number has changed. 
		
 Date:		19 September 2006
 Author:	E. Scott Daniels
 ---------------------------------------------------------------------------------
*/
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pwd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <grp.h>

#include <ningaui.h>

#include "ng_dosu.man"


				/* command 'keys' */
#define	CMD_ALIAS	1 	/* set an alias IP address for an interface */
#define CMD_UNALIAS	2	/* remove an IP alias */
#define CMD_SG_INQ83	3	/* get page 83 from tape library */
#define CMD_SG_INQ	4	/* general gnquiry from sg manager */
#define CMD_MOUNT	5	/* mount a filesystem */
#define CMD_UMOUNT	6	/* unmount a file system */
#define CMD_FSTAB	7	/* install the equerry fstab */
#define CMD_CHOWN	8	/* mount and then chown */
#define CMD_MKMDIR	9	/* make a mount point directory (last node must be rmfsxxxx) */
#define CMD_MKMTPT	10	/* add MOUNT_POINT file to dir */
#define CMD_DEVLIST	11	/* camcontrol dev list */
#define CMD_CC_INQ	12	/* camcontrol inquiry */
#define	CMD_TEST	99	/* self test */

#define UNKNOWN		0	/* style of ifconfig command syntax */
#define LINUX		1	/* linux/solaris */
#define BSD		2	/* bsd/darwin */

#define FSTAB_COOKIE	"#equerry-begin"		/* we must find this as the first token on a line in fstab to do mounts/umounts */


char *version = "2.12/0a129";
char *tdir = NULL;

struct {			/* list of supported actions */
	char	*name;		/* our command name */
	int	nargs;		/* number of arguments expected */
	int	key;		
} clist[] = {
	"alias", 	3, 	CMD_ALIAS,			/* set an alias name.domain for the interface; args = ifname netmask bcast */
	"unalias", 	1, 	CMD_UNALIAS,			/* delete an alias name.domain for the interface; args = ifname */
	"sg_inq",	1,	CMD_SG_INQ,			/* generate a generate query to sg; arg is device name */
	"sg_inq83",	1,	CMD_SG_INQ83,			/* generate a page 83 quwery; arg is device name */
	"mount",	2,	CMD_MOUNT,			/* generic mount */
	"chown",	2,	CMD_CHOWN,			/* generic mount with chown */
	"umount",	1,	CMD_UMOUNT,			/* unmount */
	"fstab",	0,	CMD_FSTAB,			/* install equerry fstab */
	"mkmdir",	1,	CMD_MKMDIR,			/* make a mount point directory */
	"mkmtpt",	1,	CMD_MKMTPT,			/* add a MOUNT_POINT tag to a directory */
	"cc_devlist",	0,	CMD_DEVLIST,			/* camcontrol devlist */
	"cc_inq",	1,	CMD_CC_INQ,			/* camcontrol inquiry */
	"test",		0, 	CMD_TEST,			/* run a simple test of dosu */
	NULL, 0, 0
};

/* inline ifdefs would be more efficent, but they make it hard to read and this is not
   going to be executed often enough to worry about the extra if's later
*/
#if defined( OS_FREEBSD ) || defined( OS_DARWIN )
#define	STYLE  BSD
#else
#if defined( OS_LINUX ) || defined( OS_SOLARIS )
#define	STYLE  LINUX
#else
#define	STYLE  UNKNOWN
#endif
#endif

void usage( )
{
	sfprintf( sfstderr, "ng_dosu version %s\n",  version );		/* we use hard-coded name on purpose! */
	sfprintf( sfstderr, "usage:\n" );
	sfprintf( sfstderr, "\tng_dosu [-v] alias <if-name>\n" );
	sfprintf( sfstderr, "\tng_dosu [-v] unalias <if-name>\n" );
	sfprintf( sfstderr, "\tng_dosu [-v] sg_inq <dev-name>\n" );
	sfprintf( sfstderr, "\tng_dosu [-v] sg_inq83 <dev-name>\n" );
	sfprintf( sfstderr, "\tng_dosu [-v] mount <device/label> <mtpt>\n" );
	sfprintf( sfstderr, "\tng_dosu [-v] umount <mtpt>\n" );
	sfprintf( sfstderr, "\tng_dosu [-v] fstab\n" );
	sfprintf( sfstderr, "\tng_dosu [-v] mkmdir <mtpt-dir-name>\n" );
	sfprintf( sfstderr, "\tng_dosu [-v] mkmtpt <mtpt-dir-name>\n" );
	sfprintf( sfstderr, "\tng_dosu [-v] cc_devlist\n" );
	sfprintf( sfstderr, "\tng_dosu [-v] cc_inq <device>\n" );
	sfprintf( sfstderr, "\tng_dosu [-v] test\n" );
	sfprintf( sfstderr, "\tng_dosu [--man]\n" );
	exit( 1 );
}

/* dig the variable from pinkpages; abort if not defined or is empty */
char *get_ppvar( char *name )
{
	char	*v;	/* raw value */
	char	*ev;	/* value with $vnames expanded */

	if( (v = ng_pp_get( name, NULL )) && *v )
	{
		ev = ng_pp_expand( v );
		ng_free( v );
		return ev;
	}

	if( (v = ng_env( name )) != NULL )
		return v;

	ng_bleat( 0, "abort: cannot find variable in pinkpages: %s", name );
	exit( 1 );
}


/* clean the environment for safety */
void wash( )
{
 	putenv( "IFS=" );			/* rather use unsetenv() but bloody sunos libc does not support */
 	putenv( "CDPATH=" );
 	putenv( "ENV=" );
 	putenv( "BASH_ENV=" );
 	putenv( "KRB_CONF=" );
 	putenv( "KRBCONFDIR=" );
 	putenv( "KRBTKFILE=" );
 	putenv( "KRB5_CONFIG=" );
 	putenv( "LOCALDOMAIN=" );
 	putenv( "RES_OPTIONS=" );
 	putenv( "HOSTALIASES=" );
 	putenv( "NLSPATH=" );
 	putenv( "PATH_LOCALE=" );
 	putenv( "TERMINFO=" );
 	putenv( "TERMINFO_DIRS=" );
 	putenv( "TERMPATH=" );
}

/* divide the command into tokens and exec overtop of us */
void doit( char *cmd )
{
	char	**tokens = NULL;
	int	ntokens = 0;

	tokens = ng_tokenise( cmd, ' ', '\\', NULL, &ntokens );			/* ng_tokenise leaves cmd buffer unchanged */
	if( ntokens )
	{
		putenv( "PATH=/sbin:/usr/sbin:/bin:/usr/bin" );			/* enforce sanity (rather setenv() but sun is odd) */
		execvp( tokens[0], tokens );						/* go to it */
		ng_bleat( 0, "abort: exec failed: %s: %s", cmd, strerror( errno ) );	
	}
	else
		ng_bleat( 0, "abort: command did not tokenise properly: %s", cmd );
	
	exit( 1 );			/* any time we get here its not good */
}


/* suss out the name of the gid; if gid < 0 then we get for current user 
*/
char *get_gname( gid_t gid )
{
	struct 	group grp;
	struct 	group *gp = NULL;
	char	buf[1024];

	if( gid > 0 )
		gid = getgid();

	memset( &grp, 0, sizeof( grp ) );
	grp.gr_gid = gid;
	*buf = 0;
#ifdef OS_SOLARIS
	gp = getgrgid_r( gid, &grp, buf, sizeof( buf ) );
#else
	getgrgid_r( gid, &grp, buf, sizeof( buf ), &gp );
#endif

	if( gp )
		return ng_strdup( gp->gr_name );
	else
		return NULL;
}

/* we require that if the binary is setuid then the group/world write read/write bits are off 
	if the binary mode settings seem right, then we return the group name that owns it
*/
char *validate_binary( char *path )
{
	struct	stat sb;

	if( stat( path, &sb ) != 0 )
	{
		ng_bleat( 0, "abort: cannot stat: %s: %s", path, strerror( errno ) );
		exit( 1 );
	}

	if( (sb.st_mode & S_ISUID) && (sb.st_mode & 0066) )		/* we insist that the read/write mode be 0 for group/other */
	{
		ng_bleat( 0, "abort: bad mode: %s", path );
		exit( 1 );
	}

	return get_gname( sb.st_gid );
}

/* validate things:
	The mode of the binary must not have read/write bits on for group/other if the suid bit is 
	on. If the suid bit is not on, then we dont care what the mode is. 

	Then, the user must be root, or the user name (e.g. ningaui) must match the name of 
	the group that owns the binary.  Thus if the binary is owned by root:foo only the user
	foo or root will be allowed to execute this binary.  It is the NAMES that are matched
	so that the uid for foo and the gid for foo do NOT need to be the same.

	If the user is legit, then we test to see that the command token is recognised.
	AND
	the number of parameters passed in (a[0]...) matches what we expect

	And 
	that each parameter does not contain any blanks or semicolons

	if all of that smells good, return the key from the clist
*/
int validate( char *cs, int c, char **a )
{
	int	i;
	int	n = 0;
	int 	clist_idx = -1;
	struct 	passwd *pwd = NULL;
	uid_t	uid;
	char	*bgroup;			/* group that owns the binary */
	char	*ugroup;			/* user's group name */



	bgroup = validate_binary( argv0 );				/* insist that the mode on the binary is 0 for other/group; get group owner name */
	
	if( !bgroup || !(pwd = getpwnam( bgroup )) )				/* see if there is a user name that matches the group that owns the file */
	{
		ng_bleat( 0, "abort: cannot find password entry for %s", bgroup ? bgroup : "bad group" );
		exit( 1 );
	}

	uid = getuid( );					/* user id that is running this process */
	if( uid != 0 && uid != pwd->pw_uid )			/* user does not  the group name on the binary and  user is not root */
	{
		ng_bleat( 0, "abort: wrong user: %d is not %s", uid, bgroup );
		exit( 1 );
	}

	for( i = 0; clist[i].name != NULL; i++ )			/* find the command in our list */
	{
		if( strcmp( clist[i].name, cs ) == 0 )
		{
			n = clist[i].nargs;
			clist_idx = i;
			break;
		}
	}

	if( clist_idx < 0 )						/* not one we do */
	{
		ng_bleat( 0, "abort: bad parameter: %s", cs );		
		exit( 1 );
	}

	if( n != c )							/* just the right number of parms */
	{
		ng_bleat( 0, "abort: bad arg list for %s (%d,%d)", cs, n, c );
		exit( 1 );
	}

	ng_bleat( 2, "ok: %s [%d]", cs, clist_idx );

	for( i = 0; i < c; i++ )					/* check out each parameter they passed in */
	{
		if( strchr( a[i], ';' ) || strchr( a[i], ' ' ) || strchr( a[i], '\t' ) )	/* no blanks or semicolons allowed */
		{									
			ng_bleat( 0, "abort: bad parameter: %s", a[i] );
			exit( 1 );
		}

		ng_bleat( 2, "ok: %s", a[i] );
	}

	return clist[clist_idx].key;					/* all smells good; return the command id constant */
}

/* validate that the fstab has been setup to allow equerry to mount 
	return 1 if ok.
*/
int validate_fstab( char *dev )
{
	Sfio_t	*f = NULL;
	char	*buf = NULL;
	char	*tok = NULL;
	int	state = 0;
	int	found = 0;
	

	if( (f = sfopen( NULL, "/etc/fstab", "r" )) != NULL )
	{	
		while( (buf = sfgetr( f, '\n', SF_STRING )) != NULL )
		{
			if( ! found )
			{
				if( strncmp( buf, FSTAB_COOKIE, 14) == 0 )
				{
					if( ! dev )				/* request to just validate that the cookie is here */
					{
						state = 1; 
						ng_bleat( 1, "equerry cookie found in fstab; device after cookie check not made" );
						break;
					}

					ng_bleat( 1, "equerry cookie found in fstab" );
					found = 1;		/* cookie found, state is good if dev is found after the cookie */
				}
			}
			else
			{
				if( strncmp( buf, "LABEL=", 6) == 0 )
					buf += 6;
				tok = strtok( buf, " \t" );
				if( strcmp( tok, buf ) == 0 )		/* device name is in table after cookie, state is good */
				{
					ng_bleat( 1, "device found after equerry cookie" );
					state = 1;
					break;
				}
			}
		}

		sfclose( f );
	}

	return state;
}

int main( int argc, char **argv )
{
	char	*cmd = NULL;				/* command token from command line */
	char	ex_cmd[NG_BUFFER];			/* command string to exec */
	int	cmd_key;				/* 'index' into switch */
	int	style = STYLE;				/* type of ifconfig format needed */
	char	*cp;					/* general pointer into character string */
	char	*cluster;
	char	*domain;					
	char	*netmask = NULL;
	char	*bcast = NULL;				/* must supply netmask and bcast to match the real interface on linux */
	char	*linux_cookie = NULL;			/* linux adds :n as an alias, the cookie is the n to use; pickup from ng_env() */

	cluster = get_ppvar( "NG_CLUSTER" );		/* these abort if not defined */
	domain = get_ppvar( "NG_DOMAIN" );
	tdir = get_ppvar( "TMP" );

	ARGBEGIN
	{
		case 'v':	OPT_INC( verbose );	break;
usage:
		default:
			usage( );
			exit( 1 );
			break;
	}
	ARGEND

	cmd = *argv++;
	argc--;
	
	if( ! cmd )
	{
		usage( );
		exit( 1 );
	}

	/*validate_binary( argv0 ); */				/* insist that the binary is 0 for other/group */
	if( (cmd_key = validate( cmd, argc, argv )) < 0  )	/* validate the command */
		exit( 1 );

	linux_cookie = ng_env( "NG_DOSU_ALIASID" );		/* get early; we use env, not pp, so srv_netif script can add on command line */
	wash( );						/* remove potentially risky things from the environment */

	ex_cmd[0] = 0;
	switch( cmd_key )					/* build command */
	{
		case CMD_ALIAS:
			if( style == BSD )		/* darwin/bsd */
				sfsprintf( ex_cmd, sizeof( ex_cmd ), "ifconfig %s add %s.%s netmask 255.255.255.255", argv[0], cluster, domain );
			else
			{
				if( style == LINUX )
				{
					netmask = argv[1];
					bcast = argv[2];
					sfsprintf( ex_cmd, sizeof( ex_cmd ), "ifconfig %s:%s netmask %s broadcast %s %s.%s", argv[0], linux_cookie ? linux_cookie : "2",  netmask, bcast, cluster, domain );
				}
				else
				{
					ng_bleat( 0, "alias is unsupported on this o/s" );
					exit( 1 );
				}
			}

			ng_bleat( 1, "adding IP alias to interface: %s", ex_cmd  );
			ng_log( LOG_INFO, "adding IP alias to interface: %s", ex_cmd  );
			break;

		case CMD_UNALIAS:
			if( style == BSD )
				sfsprintf( ex_cmd, sizeof( ex_cmd ), "ifconfig %s -alias %s.%s", argv[0], cluster, domain );
			else
			{
				if( style == LINUX )
					sfsprintf( ex_cmd, sizeof( ex_cmd ), "ifconfig %s:%s down", argv[0], linux_cookie ? linux_cookie : "2"  );
				else
				{
					ng_bleat( 0, "unalias is unsupported on this o/s" );
					exit( 1 );
				}
			}

			ng_bleat( 1, "removing IP alias from interface: %s", ex_cmd  );
			ng_log( LOG_INFO, "removing IP alias to interface: %s", ex_cmd  );
			break;

		case CMD_SG_INQ:
			sfsprintf( ex_cmd, sizeof( ex_cmd ), "sg_inq %s", argv[0] );
			ng_bleat( 1, "%s", ex_cmd );
			break;

		case CMD_SG_INQ83:
					/* there seem now to be two flavours of this command, try to deduce which */
			if( system( "sg_inq -? 2>&1|grep \"opcode_page\" >/dev/null 2>&1" ) )		/* new one if this fails (rc==1) */
			{
				ng_bleat( 1, "new sg_inq page 83 command syntax necessary" );
				sfsprintf( ex_cmd, sizeof( ex_cmd ), "sg_inq -r -e -p 0x83 %s", argv[0] );
			}
			else
				sfsprintf( ex_cmd, sizeof( ex_cmd ), "sg_inq -r -e -o=83 %s", argv[0] );

			ng_bleat( 1, "%s", ex_cmd );
			break;

		case CMD_DEVLIST:						/* camcontrol device list */
			if( style != BSD )
			{
				ng_bleat( 0, "cannot execute this ng_dosu subcommand on this system type [FAIL]" );
				exit( 1 );
			}
			sfsprintf( ex_cmd, sizeof( ex_cmd ), "camcontrol devlist" );
			ng_bleat( 1, "%s", ex_cmd );
			break;

		case CMD_CC_INQ:
			if( style != BSD )
			{
				ng_bleat( 0, "cannot execute this ng_dosu subcommand on this system type [FAIL]" );
				exit( 1 );
			}
			sfsprintf( ex_cmd, sizeof( ex_cmd ), "camcontrol inquiry %s", argv[0], argv[1] );
			ng_bleat( 1, "%s", ex_cmd );
			break;


		case CMD_CHOWN:	
			setreuid(geteuid(), geteuid());			
			sfsprintf( ex_cmd, sizeof( ex_cmd ), "chown %s %s",  argv[0], argv[1] );	
			ng_bleat( 1, "%s", ex_cmd );
			break;

		case CMD_MOUNT:
			setreuid(geteuid(), geteuid());			/* needed as linux mount checks real uid */
			if( validate_fstab( argv[0] ) )
			{
				sfsprintf( ex_cmd, sizeof( ex_cmd ), "mount %s", argv[1] );	/* we mount using mtpt name, not device/label */
				ng_bleat( 1, "%s", ex_cmd );
			}
			else
			{
				ng_bleat( 0, "unable to validate fstab" );
				exit( 1 );
			}
			break;

		case CMD_UMOUNT:
			setreuid(geteuid(), geteuid());			/* needed as linux mount checks real uid */
			if( validate_fstab( NULL ) )		/* confirm that equerry magic cookie is there */
			{
				sfsprintf( ex_cmd, sizeof( ex_cmd ), "umount %s", argv[0] );
				ng_bleat( 1, "umount cmd = (%s)", ex_cmd );
			}
			else
			{
				ng_bleat( 0, "unable to validate fstab" );
				exit( 1 );
			}
	
			break;

		case CMD_FSTAB:
			if( validate_fstab( NULL ) )			/* check just for the cookie */
			{
				char	new_fstab[1024];

				sfsprintf( new_fstab, sizeof( new_fstab ), "%s/PID%d.fstab", tdir, getppid( ) );	
				sfsprintf( ex_cmd, sizeof( ex_cmd), "cp %s /etc/fstab", new_fstab );
				ng_bleat( 1, "%s", ex_cmd );
			}
			else
			{
				ng_bleat( 0, "unable to validate fstab" );
				exit( 1 );
			}
			break;

		case CMD_MKMDIR:			/* creates the directory provided name is of the form: /xxx/yyy/rmfs* */
			if( (cp = strrchr( argv[0], '/' )) != NULL )
			{
				if( strncmp( cp+1, "rmfs", 4 ) == 0 || strncmp( cp+1, "fs", 2 ) == 0  ||
				 	strncmp( cp+1, "pkfs", 4 ) == 0 || strncmp( cp+1, "pk", 2 ) == 0  || strncmp( cp+1, "ng_", 3 ) == 0 ) 
				{
					if( mkdir( argv[0], 0775 ) != 0 )
					{
						ng_bleat( 0, "unable to mkdir: %s: %s", argv[0], strerror( errno ) );
					}
					else
					{
						ng_bleat( 1, "directory created: %s", argv[0] );
						ng_log( LOG_INFO, "mtpt directory created: %s", argv[0]  );
					}
					exit( 0 );
					
				}
				else
				{
					ng_bleat( 0, "bad directory name (trailing /rmfs* missing): %s", argv[0] );
					exit( 1 );
				}
			}
			else
			{
				ng_bleat( 0, "bad directory name (no path): %s", argv[0] );
				exit( 1 );
			}
			break;

		case CMD_MKMTPT:			/* adds MOUNT_POINT to directory provided name is of the form: /xxx/yyy/rmfs* */
			if( (cp = strrchr( argv[0], '/' )) != NULL )
			{
				if( strncmp( cp+1, "rmfs", 4 ) == 0 || strncmp( cp+1, "fs", 2 ) == 0  ||
				 	strncmp( cp+1, "pkfs", 4 ) == 0 || strncmp( cp+1, "pk", 2 ) == 0  || strncmp( cp+1, "ng_", 3 ) == 0 ) 
				{
					sfsprintf( ex_cmd, sizeof( ex_cmd), "touch %s/MOUNT_POINT", argv[0] );
					ng_log( LOG_INFO, "creating mountpoint tag with: %s", ex_cmd );
					
				}
				else
				{
					ng_bleat( 0, "bad directory name (trailing /rmfs* missing): %s", argv[0] );
					exit( 1 );
				}
			}
			else
			{
				ng_bleat( 0, "bad directory name (no path): %s", argv[0] );
				exit( 1 );
			}
			break;

		case CMD_TEST:
			if( validate_fstab( argv[0] ) )
				ng_bleat( 0, "fstab validataion passed" );
			else
				ng_bleat( 0, "fstab validation failed" );

			ng_bleat( 0, "running test command (ls -al)" );
			sfsprintf( ex_cmd, sizeof(ex_cmd), "ls -al" ); 
			break;

		default:
			ng_bleat( 0, "internal error: invalid command key pair: cmd=%s key=%d", cmd, cmd_key );		/* impossible? */
			exit( 1 );
			break;
	}

	doit( ex_cmd );		/* send it off; we do not expect a return */
	exit( 1 );		/* safety net */
}


/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_dosu:Ningaui as root command set)

&space
&synop	ng_dosu [-v] action [parms...]
&space	Specificly, actions and parms are:
&break	alias if-name netmask
&break	unalias if-name
&break	sg_inq dev-name
&break	sg_inq83 dev-name
&break	mount <device/label> <mtpt>
&break	umount <mtpt>
&break	fstab
&break	mkmdir <mtpt-dir>
&break	mkmtpt <mtpt-dir>
&break	cc_devlist 
&break	cc_inq device
&break	test

&space
&desc	&This executes as a set uid root programme allowing a small set of actions
	to be executed as the super user.  The actions are invoked via options and 
	parameters rather than command name. These actions are supported:
&space
&beglist
&item	Add an alias IP address to an interface. The netmask is required for Linux
	nodes.  If supplied, and &this is executed on a FreeBSD node the netmask
	is ignored. 
&space
&item	Delete an alias from an interface.
&space
&item	Make an inquiry request from the general scsi device driver for page
	83 data from the indicated device.  The output of this command is in binary
	and thus piping it throug &lit(od) or other process that can properly 
	translate the data is required. 
&space
&item	Make an inquiry request from the genneral scsi device driver for the indicated device.
&endlist

&space
&subcat	Requirements
	Several validation checks must be met before &this will operate:
&space
&beglist
&item	It is assumed that the binary has a set-uid bit turned on. If so, then 
	the mode of the binary must NOT have the read or write bits turned on 
	for either the group or other users. 
&space
&item	The  production user-id must be used to inovoke the command. (see note
	below)

&space
&item	The command must be executed with a fully qualified pathname.

&space
&item	The parameters on the command line which follow the action token
	must not have any embedded whitespace, or semicolons, and must be 
	the expected number for the action. 
&endlist

&space
&subcat	Dependant Software/Functions
	It might be against some installations policy to allow set uid programmes that 
	are owned by root.  If &this is not installed as a suid root binary, the following
	programmes and/or functions will not function correctly:
&space
&beglist
&item	Network interface (IP takeover) service (ng_srv_netif).
&item	Equerry (ng_equerry and ng_eqreq)
&item	Tape  (ng_tp_*)
&endlist

&space
&subcat	NOTE:
	The binary ng_FIXUP_dosu is generated and included with potoroo pacakges.
	This ensures that when the package is installed with
	an ng_refurb command, it does not automatically overlay the suid-root copy of ng_dosu
	which will revert permissions to ningaui.  The system manager must copy 
	ng_FIXUP_dosu to ng_dosu and set the permissions; required only when the 
	version number has changed. 
	
&space
	Further, the group owner of the binary MUST match the name of the production 
	user-name (usually ningaui).  For instance, if the user name agama is being used
	as the cluster production user id, then the &lit(agama) group must also exist
	and the ng_dosu binary must have the group owner set to &lit(agama.) The 
	check is made using the group names, so the gid and the uid do NOT need to 
	match. 

&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-v : Verbose mode.  
&endterms

&space
&parms	The parameters expected by &this are dependant on the command token which is 
	the required first parameter.  The  following describes the command tokens
	that are recognised, the action(s) taken by &this, and any necessary 
	command line parameters that must follow the token.
&space
&begterms
&term	alias : Causes an IP alias of <cluster-name>.<domain-name> to be added to the 
	supplied interface name. The cluster and domain names are assumed to be set 
	in pinkpages (NG_CLUSTER and NG_DOMAIN). 
&space
	Linux adds alias IP addresses by creating a new interface name (e.g. eth0:2) 
	By default we use 2 as the extension, but this can be controlled with the 
	NG_DOSU_ALIATID pinkpages variable; if unset we use 2.
&space
&term	unalias : Removes an IP alias that was assigned to the supplied interface name. 
&space
&term	cc_devlist : Executes a &lit(camcontrol devlist) command. This currently is legitimate
	only on FreeBSD systems.
&space
&term	cc_inq : Executes a &lit(camcontrol querry) for the indicated device. Like &lit(cc_devlist,)
	this command is legitimate only on FreeBSD systems.
&space
&term	chown : Execute a chown command.
&space
&term	sg_inq : Invoke sg_inq to generate an inquiry to the general scsi driver requesting
	data from the named device. 
&space
&term	sg_inq83 : Invoke sg_inq to generate an inquiry to the general scsi driver requesting
	page 83 data from the named device.  The data is output in binary.
&space
&term	mount : Accepts a device and mountpoint and attempts to mount the fileystem. Before 
	mounting, the special &lit(ng_equerry) cookie is validated in the fstab, and the 
	indicated mountpoint must be defined after the cookie in the file. If the cookie, 
	and the position of the filesystem in the file are not legit, then no attempt 
	to mount the filesystem is made. 
&space
&term	mkmdir : Creates the directory provided that the directory name has the form:
	&lit(/xxx/yyy/rmfs*).  This is for creating replication manager mount point directories
	on the fly when the mountpoint is not in $NG_ROOT.
&space
&term	mkmtpt : Adds the MOUNT_POINT file to  the directory provided that the directory name has the form:
	&lit(/xxx/yyy/rmfs*).  This is for creating replication manager mount point directories
	on the fly when the mountpoint is not in $NG_ROOT.
&space
&term	umount : Unmounts the named filesystem. The same fstab verification that is done for
	mounting before any attempt to unmount a filesystem is made. 
&space
&term	test : Causes &this to execute an &lit(ls -al) command in the current directory
	as a simple test of the binary.
&endterms

&space
&exit	&This exits with a non-zero return code if it is unable to setup or execute
	the desired command. The underlying command's return code is returned to 
	the user if &this the exec() system call is successful.

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	19 Sep 2006 (sd) : Its beginning of time. 
&term	27 Sep 2006 (sd) : Mod to deal with the fact that the Sun libc seems not 
		to support unsetenv() nor setenv(); converted to putenv() calls.
&term	14 Feb 2007 (sd) : Changed to set netmask on linux nodes. The default
		255.255.255.255 that was used on BSD nodes is just fine. 
&term	16 May 2007 (sd) : Now accept the linux cookie (:n) allowing us to unalias
		an interface name that was not set using our preferred number.
&term	02 Jul 2007 (sd) : Added the sg_inq commands.  (v2.2)
&term	19 Jul 2007 (sd) : Fixed cause of coredump if no args given. (v2.3)
&term	22 Jul 2007 (sd) : Fixed cause of coredump when &this was not able to stat the binary to 
		verify permissions. (v2.4)
&term	12 Sep 2007 (sd) : Root is now permitted to execute this.  (v2.5)
&term	10 Oct 2007 (sd) : Fixed the ability to let root run this too. (v2.6)
&term	31 Jan 2008 (sd) : Added a set real user id for mount/unmount so that it will work with Linux's mount command. (v2.7)
&term	29 Feb 2008 (sd) : Added chown command (v2.8)
&term	07 Apr 2008 (sd) : Added support for creating rmfs mountpoint directory and mountpoint tag. 
&term	17 Apr 2008 (sd) : Changed mountpoint tag support to include pk and pkfs filesystems.
&term	12 Jun 2008 (sd) : Allows ng_ mtpt directories and mtpt tags to be created.
&term	03 Jul 2008 (sd) : Added camcontrol device list and inq abilities. (v2.10)
&term	11 Jul 2008 (sd) : Added ability to use a prod-id other than ningaui. (v2.11)
&term	14 Jul 2008 (sd) : Had to tweek to allow compile under sunos. (version not changed)
&term	12 Oct 2009 (sd) : Tweeked the sg_inq command to work on new suse nodes. (v2.12) 
&endterms


&scd_end
o----------------------------------------------------------------- */
