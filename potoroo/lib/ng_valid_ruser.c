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
-----------------------------------------------------------------------------------
  Mnemonic:	ng_valid_ruser
  Abstract:	accepts a username and host name and determines whether 
		that user is considered a valid user on this node.  The validation 
		process is similar to that of the r-commands use of the .rhosts
		file. 
		=== see self contained doc for the complete details =====
 		
  Date:		22 April 2008
  Author:	E. Scott Daniels
  ---------------------------------------------------------------------------------
*/


#include <sys/types.h>
#include <pwd.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include <sfio.h>
#include <ningaui.h>

static char *vu_version="valid_user v1.1/05018";
int ng_valid_ruser( char *runame, char *rhost )
{
	static	char *my_uname = NULL;		/* user name executing this process */
	static	char *ue_file = NULL;		/* user equiv (authorisaation) file */
	char	wbuf[NG_BUFFER];
	char	*strtok_p;
	char	*rcluster = NULL;		/* remote cluster name */
	char	*gp;				/* general pointer to something temporary */
	struct	stat stat_data;
	
	if( ! my_uname )
	{
		struct passwd pw;
		struct passwd *pwr = 0;
		char	*ng_root;
 
#ifdef OS_SOLARIS
	        if( ! getpwuid_r( geteuid(), &pw, wbuf, sizeof( wbuf ) ) )			/* bloody sunos has to be different */
#else
	        if( getpwuid_r( geteuid(), &pw, wbuf, sizeof( wbuf ), &pwr ) )
#endif
		{
			ng_bleat( 0, "ng_valid_ruser: cannot determine my username (uid=%d) -- cannot validate: %s@%s: %s", geteuid(), runame, rhost, strerror( errno ) );
			return 0;
		}
		my_uname = ng_strdup( pw.pw_name );	

		if( (ng_root = ng_env( "NG_ROOT")) == NULL )
		{
			ng_bleat( 0, "ng_valid_ruser: cannot determine NG_ROOT -- cannot validate: %s@%s", runame, rhost );
			return 0;
		}

		sfsprintf( wbuf, sizeof( wbuf ), "%s/site/%s.uequiv", ng_root, pw.pw_name );
		ue_file = ng_strdup( wbuf );

		ng_nar_close( );
		ng_free( ng_root );
	}

	if( !rhost || !runame )
	{
		ng_bleat( 0, "ng_valid_ruser: missing information; user not authorised" );
		return 0;
	}
	
	if( (rcluster = ng_pp_node_get( "NG_CLUSTER",  rhost )) == NULL )
	{
		rcluster = "null";
		ng_bleat( 3, "ng_valid_user: rhost does not belongs to any cluster" );
	}
	else
		ng_bleat( 3, "ng_valid_user: rhost belongs to %s", rcluster );

	if( stat( ue_file, &stat_data ) == 0 )
	{
		/* file owned by local user, not a link, and not read/writable by group/others */
		if( stat_data.st_uid == geteuid()  && (stat_data.st_mode & S_IFREG) == S_IFREG &&  (stat_data.st_mode & 0x3f) == 0 )
		{
			char	*b;
			char	*tok;
			char	*tname;			/* name (cluster or host) to test against */
			int	negate = 0;
			int	ok = 0;
			long	rec = 0;
			Sfio_t	*f;

			if( (f = sfopen( NULL, ue_file, "r" )) != NULL )
			{
				while( (b = sfgetr( f, '\n', SF_STRING )) != NULL )
				{
					rec++;
					tok = strtok_r( b, " \t", &strtok_p );
					if( tok && *tok != '#' )			/* skip blank lines and comment lines */
					{
						if( *tok == '-' )
						{
							negate = 1;
								tok++;
						}
						else
							{
							negate = 0;
							if( *tok == '+' )
								tok++;
						}
	
						if( ! *tok )
						{
							ng_bleat( 3, "ng_valid_ruser: [%d] lone +/- in eq file: %s(file) %s access %s(ruser) %s(rhost) %s(rcluster)", rec, ue_file, negate ? "deny" : "allow", runame, rhost, rcluster );
							sfclose( f );
							return !negate; 		/* just a + or - and no host name allow/deny all */
						}
	
						switch( *tok )	/* select remote name to test this entry against */
						{
							case 'c':
							case 'C':
								tname = rcluster;
								break;

							case 'h':
							case 'H':
								tname = rhost;
								break;

							default:
								tname = "\n";
								ng_bleat( 0, "invalid entry in %s near %s; skipped", ue_file, b );
								break;
						}

						if( *(tok+1) != ':' )
						{
							tname = "\n";
							ng_bleat( 0, "invalid entry in %s near %s; skipped", ue_file, b );
						}
						else
							tok += 2;
						if( *tok == '*' || strcmp( tok, tname ) == 0 )		/* cluster/host matches this entry */	
						{
							if( (tok = strtok_r( NULL, " \t,", &strtok_p )) != NULL )	/* search list of unames for our runame */
							{
								while( tok && strcmp( tok, runame ) )
									tok = strtok_r( NULL, " \t,", &strtok_p );
	
								
								if( tok )			/* found user, send back now */
								{
									sfclose( f );
									ng_bleat( 3, "ng_valid_ruser: [%d] matched in eq file: %s(file) %s (rluster) %s(rost) %s(ruser) %s access", rec, ue_file, rcluster, rhost, runame, negate ? "deny" : "allow" );
									return !negate;
								}
							}
							else
							{
								ng_bleat( 3, "ng_valid_ruser: [%d] matched (any user) in eq file: %s(file) %s (rcluster) %s(rhost) %s access", rec, ue_file, rcluster, rhost, runame, negate ? "deny" : "allow" );
								sfclose( f );
								return !negate;
							}
						}
					}
				}

				ng_bleat( 3, "ng_valid_ruser: [%d] neither rcluster nor rhost were matched in eq file: %s(file) %s(host)", rec, ue_file, rhost );
				sfclose( f );
			}
		}
		else
			ng_bleat( 3, "ng_valid_ruser: bad ue_file: %s(file) %d(uid) %02x(type) %03o(mode)",  ue_file, stat_data.st_uid, stat_data.st_mode & S_IFMT, stat_data.st_mode & 0xfff );
	}
	else
	{
		/* cannot stat file; so we assume it had one entry: -null */
		if( strcmp( rcluster, "null" )  != 0 )
		{
			ng_bleat( 3, "ng_valid_ruser: access allowed for %s. cannot stat ue_file: %s(file) and host (%s) is in known cluster %s", runame, ue_file, rhost, rcluster );
			return 1;
		}

		ng_bleat( 3, "ng_valid_ruser: user %s rejected. cannot stat ue_file: %s(file) and host (%s) not in known cluster", runame, ue_file, rhost );
	}

	ng_bleat( 2, "ng_valid_ruser: deny ruser(%s) from %s", runame, rhost );
	return 0;
}

#ifdef SELF_TEST
int verbose = 3;
int main( int argc, char **argv )
{
	int c;

	c = ng_valid_ruser( argv[1], argv[2] );
	printf( "user was %s\n", c ? "allowed" : "rejected!" );

}
#endif



/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_valid_ruser:Validate remote user)

&space
&synop	int ng_valid_ruser( char *uname, char *rhost )

&space
&desc
  	&This accepts a username and host name and determines whether 
	that user is considered a valid user on this node.  The validation 
	process is similar to that of the r-commands use of the .rhosts
	file. 
		
&space
	There are some differences. The user equiv file is newline seprated 
	records of the form:
&space
&litblkb
   [+|-]{h|c}:name [user[,user...]]
&litblke

&space
		The plus/minus sign indicate if the entry serves to allow (+) or 
		deny (-) users. 

&space
		The leading c or h on the second token indiicates whether the name 
		is a cluster or node name.  The cluster or host name can be coded
		as "*" which matches any. 

&space
		If no users are supplied, then the entry matches for all users. 
		If more than one user is supplied, they are supplied as a comma
		separated list. 


&space
		The host name passed in is used to look up a defined NG_CLUSTER
		pinkpages variable which is used when processing cluster entries.
		If the host does not map to a cluster, then the string "null" is
		used and thus can be specifically coded for in the equiv file. 


&space
		The user equiv file must be a regular file, owned by the same user 
		as is executing the process that has invoked this function (as returned 
		by the geteuid() function).  Further, the mode of the file must be 
		such that the file cannot be read or written to by group members
		or other users. 

&space
&space

		Authorisation of a user is determined with the following logic:
		
&space
&begterms
&term 	1 : If no equiv file exists, or it has the wrong mode setting, 
	the user is considered to be  valid provided that the remote host
	can be mapped to a known cluster.   This is similar to the original
	ningaui (lax) security model; it proivdes only a minimal amount 
	of security in that a user must have access to a cluster node
	to be considered valid. 

&space
&term	2 :  If the equiv file exists, it is searched until the first 
	entry is found that matches host or cluster and the user name if 
	the entry has a user list.
	If the mathcing entry is a positive entry (+) then 
	the user is considered valid. If the entry is negative, or there
	is not an entry that matches for the user, then the user is rejected.

		
&space
&term	3 : If no entry is matched, then the user is rejected. 


&subcat File Location	
	The user equiv file is expected to be in the &lit(NG_ROOT/site) directory.
	The name of the file is constructed using the user name of the process 
	owner (pname) and is assumed then to have the syntax:
&space
&litblkb
   <pname>.uequiv
&litblke


&space
&exit
	If a user is considered a valid user, then a non-zero return code (true) is 
	returned. A return code of zero indicates that the user was not approved. 
	If the global verbose setting is 3 or larger, this function writes rejection
	information to the standard error device.  

.if [ 0 ] 
&space
&see
	&seelink(tool:ng_rcmd)
	&seelink(zoo:ng_parrot)
.fi

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	22 Apr 2008 (sd) : Its beginning of time. 
&term	01 May 2008 (sd) : Corrected for use on solaris (getpwuid_r() is different).
&term	08 Jan 2009 (sd) : Tweeked the man page.
&endterms


&scd_end
------------------------------------------------------------------ */

