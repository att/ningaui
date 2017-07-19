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
--------------------------------------------------------------------------
*
*  Mnemonic: ng_netif.c - Ningaui network interface routines
*  Abstract:	Thse routines supply a generalised set of routines that 
*           	applications can use to interface with the network (TCP/IP 
*           	or UDP/IP). (currently basic TCP/IP support routines are 
#		still in their gecko locations or ng_rx and ng_srv; someday
#		they will be moved here.  There is also the ningaui socket
#		interface library (ng_si routines) that implement a callback
#		environment for managing concurrent TCP/IP sessions and 
#		UDP/IP traffic.
*  Date:     29 March 2001
*  Author:   E. Scott Daniels
*
*  Modified: 13 Apr 20001 - added writeaudp, and getaddr
*
*  CAUTION: The netdb.h header file is a bit off when it sets up the 
*           hostent structure. It claims that h_addr_list is a pointer 
*           to character pointers, but it is really a pointer to a list 
*           of pointers to integers!!!
*       
------------------ SCD AT END --------------------------------------------
*/
#include	<sys/types.h>          /* various system files - types */
#include	<sys/ioctl.h>
#include	<sys/socket.h>         /* socket defs */
#include 	<netinet/in.h>
#include	<stdlib.h>
#include 	<arpa/inet.h>
#include	<string.h>
#include	<unistd.h>
#include	<ctype.h>

#include	<net/if.h>
#include	<stdio.h>              /* standard io */
#include	<errno.h>              /* error number constants */
#include	<fcntl.h>              /* file control */
#include	<string.h>
#include	<netinet/in.h>         /* inter networking stuff */
#include	<netdb.h>              /* for getxxxbyname calls */
#include	<string.h>

#ifdef	OS_SOLARIS
#include	<sys/sockio.h>		/* needed for sun */
#endif
#include	<sys/time.h>		/* needed for linux */
#include	<sys/select.h>		/* needed for linux */

#include	<sfio.h>  
#include	"ningaui.h"

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL	0	/* systems like SGI dont support this */
#endif
				/* conversion types for convert */
#define AC_TODOT 	0	/* addr struct to dotted string */
#define AC_TOADDR 	1	/* dotted string to addr struct */
#define AC_NETTODOT 	2	/* addr to dotted string network only */

static int convert( void *, void *, int );
char *ng_addr2dot( struct sockaddr_in *src );
struct sockaddr_in *ng_dot2addr( char *src );
char *ng_ip2name( char *ipstring );			/* take ip address and convert to host name */

/* accepts an ip address string and converts it to a host name 
   if the resulting name is localhost, we translate that to the current system name
*/
char *ng_ip2name( char *ipstring )
{
	struct hostent *hinfo;
	char	*name = NULL;
	unsigned char bipa[256];			/* getby... wants an array of binary not a string -- inconsistant */
	char	*tok;
	int	i;

	tok  = ipstring;
	for( i = 0; i < 4; i ++ )
	{
		bipa[i] = atoi( tok );
		tok  = strchr( tok, '.' ) + 1;
	}
	
	if( (hinfo = gethostbyaddr( bipa, 4,  AF_INET )) != NULL )
		name = ng_strdup( hinfo->h_name );
	if( name && strncmp( name, "localhost", 9 ) == 0 )
	{
		ng_free( name );
		ng_sysname( bipa, sizeof( bipa ) );
		name = ng_strdup( bipa );
	}

	return name;
}

/* open a udp socket on a particular interface (net). net can be 00.00.00.00 for any interface */
int ng_open_netudp_r( char *net, int port, int reuse_port )
{
	char buf[100];               /* work buffer to build address in */
	int fd;                      /* file descriptor from the socket call */
	int optflag = 1;		/* "set" flag for sockopt call */
	struct sockaddr_in *addr;    /* address for bind call */

	if( (fd = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP )) >= 0 && port >= 0 )	/* -port results in just the socket -- no bind */
	{
#if defined(OS_FREEBSD)
  		setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &reuse_port, sizeof( reuse_port ) ) ;	/* set, allows multiple procs for mcast */
#else
  		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse_port, sizeof( reuse_port ) ) ;	/* set, allows multiple procs for mcast */
#endif

		if( net )		/* if null, dont bind */
		{
			sprintf( buf, "%s;%d", net, port ); 
			if( (addr = ng_dot2addr( buf )) != NULL )
			{
				if( bind( fd, (struct sockaddr *) addr, sizeof( *addr ) ) < 0 )
				{
					close( fd );
					fd = -1;
				}
				else
					setsockopt( fd, SOL_SOCKET, SO_BROADCAST, &optflag, sizeof( int ) ); 
	
				ng_free( addr );
			}
			else
			{
				ng_bleat( 0, "ng_netif: open_netudp: bad bind address conversion buf = %s", buf );
				close( fd );
				fd = -1;
			}
		}
	}

	return fd;
}

/* support old interfaces */
int ng_open_netudp( char *net, int port )
{
	return ng_open_netudp_r( net, port, 0 );		/* support old interface -- no reuse */
}

int ng_openudp( int port )				/* old interface -- always assumed any if card would do */
{
	return ng_open_netudp_r( "00.00.00.00", port, 0 );
}

/* join a multi cast group using a currently open UDP fd.
	group is something like 235.0.0.7  
	iface is likely the constant:  INADDR_ANY 
   returns true on success
*/
#ifdef OS_SOLARIS
typedef unsigned int u_int32_t;
#endif
int ng_mcast_join( int fd, char *group, ng_uint32 iface, unsigned char ttl )
{
	struct ip_mreq mreq;
 
        mreq.imr_multiaddr.s_addr = inet_addr(group);			/* group to join */
        mreq.imr_interface.s_addr = htonl( iface );			/* interface to listen on */
	errno = 0;
        if(setsockopt(fd, IPPROTO_IP,IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
		return 0;

	if( ttl != 0 )
        	if(setsockopt(fd, IPPROTO_IP,IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0)
		{
			ng_bleat( 0, "ng_mcast_join: setsockopt failed to set ttl" );
			return 0;
		}

	return 1;
}


/* leave a group -- if closing the fd all toghether this is not needed */
int ng_mcast_leave( int fd, char *group, ng_uint32 iface )
{
	struct ip_mreq mreq;
 
        mreq.imr_multiaddr.s_addr = inet_addr(group);			/* group to leave */
        mreq.imr_interface.s_addr = htonl( iface );			
        if(setsockopt(fd, IPPROTO_IP,IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
		return 0;

	return 1;
}

/* simple enough, but to be consistant we will add it */
void ng_closeudp( int fd )
{
	close( fd );
}

/* write buffer to given address */
int ng_writeudp( int fd, char *to, char *buf, int len )
{
	struct sockaddr_in *addr;	
	int wrote = 0;               /* bytes actually written */
	
	if( fd >= 0 && (addr = ng_dot2addr( to )) != NULL )
	{
		wrote = sendto( fd, buf, len, MSG_NOSIGNAL,  (struct sockaddr *) addr, sizeof( *addr ) ); 
		ng_free( addr );
	}

	return wrote;
}

/* faster than write because address already converted */
int ng_write_a_udp( int fd, struct sockaddr_in *to, char *buf, int len )
{
	 return sendto( fd, buf, len, MSG_NOSIGNAL, (struct sockaddr *) to, sizeof( *to ) );
}


/* read from udp port waiting up to delay seconds, or forever if delay is < 0 */
int ng_readudp( int fd, char *buf, int len, char *from, int delay )
{

	int status = 0;                /* assume the worst to return to caller */
	fd_set readfds;                /* fread fd list for select */
	fd_set execpfds;              /* exeption fd list for select */
	struct timeval *tptr = NULL;   /* time info for select call */
	struct timeval time;
	struct sockaddr uaddr;        /* pointer to udp address from recvfrom call */
	int addrlen;                   /* filled in by recvfrom call */
	char	*fc;			/* pointer at from address converted */


	FD_ZERO( &readfds );               /* clear select info */
	FD_SET( fd, &readfds );     /* set to check read status */

	FD_ZERO( &execpfds );               /* clear select info */
	FD_SET( fd, &execpfds );     /* set to check read status */

	if( delay >= 0 )                /* user asked for a fininte time limit */
	{
		tptr = &time;                 /* point at the local struct */
		tptr->tv_sec = delay;             /* setup time for select call */
		tptr->tv_usec = 0;                /* micro seconds */
	}

	if( (select( fd + 1, &readfds, NULL, &execpfds, tptr ) >= 0 ) ) 
	{		                                /* poll was successful - see if data ? */
		if( FD_ISSET( fd, &execpfds ) )   	/* session error? */
			return -3;
		else
		{
			addrlen = sizeof( uaddr );

			if( FD_ISSET( fd, &readfds ) )             /* buffer to be read */
			{                                       
				status = recvfrom( fd, buf, len, 0, &uaddr, &addrlen );
				if( from )
				{
					if( (fc = ng_addr2dot( (struct sockaddr_in *)&uaddr )) != NULL )
					{
						strcpy( from, fc );
						free( fc );
					}
					/*convert( &uaddr, from, AC_TODOT ); */
				}
			}                                       
			else                                    /* no event was received  */
				status = 0;                     /* status is just ok */

		}                                  /* not a session error */
	}                     

	return  status;            /* send back the status */
}                                 /* readudp */

/* get the broadcast address that is associated with the if name passed in */
char *ng_getbroadcast( int fd, char *ifname )
{
	struct ifreq r;
	struct sockaddr_in *sa;
	char *p;


        strncpy( r.ifr_name, ifname, IFNAMSIZ );           /* put in user supplied name */
        if( (p = strchr( r.ifr_name, ' ' )) )
                *p = 0;

	if( ioctl( fd, SIOCGIFBRDADDR, &r ) >= 0 )         /* get the address */
	{
		sa = (struct sockaddr_in *) &r.ifr_broadaddr;
		if( (p = inet_ntoa( sa->sin_addr )) )		/* convert address to dotted */
		{
			ng_bleat( 7, "getbcast: %s", p );
			return ng_strdup( p );
		}

		/*
		return ng_addr2dot( (struct sockaddr_in *) &r.ifr_broadaddr );
		convert( &(r.ifr_broadaddr), buf, AC_NETTODOT ); 
		return ng_strdup( buf );
		*/
	}

	return NULL;
}

/* convert socket address struct info into ipaddr.port */
char *ng_addr2dot( struct sockaddr_in *src )
{
	char dstr[2048];           /* excessive, but should not be overrun */
	char	*p;

	if( ! src )
		return NULL;

	if( (p = inet_ntoa( src->sin_addr )) )		/* convert address to dotted */
	{
		sfsprintf( dstr, sizeof( dstr ), "%s.%u", p, htons( src->sin_port ) );	/* add port to the ip address */
		return ng_strdup( dstr );
	}

	return NULL;

	/* return convert( src, dstr, AC_TODOT ) ? ng_strdup( dstr ) : NULL; */
}

/* convert dotted decimal string to an address structure */
struct sockaddr_in *ng_dot2addr( char *src )
{
	struct sockaddr_in *a = NULL;

	a = (struct sockaddr_in *) ng_malloc( sizeof( struct sockaddr_in ), "dot2addr:addr_in" );
	if( convert( src, a, AC_TOADDR ) )
		return a;
 
	ng_free( a );
	return NULL;
}


/* original routine - was a part of the SI callback oriented socket interface package */
static int convert( void *src, void *dest, int type )
{
 struct sockaddr_in *addr;       /* pointer to the address */
 char *dstr;                     /* pointer to dotted decimal string */
 char *tok;                      /* token pointer into dotted list */
 char *proto;                    /* pointer to protocol string*/
 struct hostent *hip;            /* host info from name server */
 struct servent *sip;            /* server info pointer */
 char *strtok_p;		 /* pointer for strtok_p to remember things by */


	if( type == AC_TOADDR )         /* from dotted decimal to address */
	{
		dstr = ng_strdup( src );
		addr = (struct sockaddr_in *) dest;                   /* set pointers */
		memset( addr, 0, sizeof( *addr ) );			/* ensure empty */
		addr->sin_addr.s_addr =  0;
		addr->sin_port = 0;              			/* incase things fail */

		if( ! isdigit( *dstr ) )       				/* assume hostname:port */
		{
			tok = strtok_r( dstr, ";: ", &strtok_p );     	/* @ first token; dot NOT a sep here */
			if( (hip = gethostbyname( tok )) != NULL )  	 /* look up host */
			{
				addr->sin_addr.s_addr =  *((int *) (hip->h_addr_list[0]));  /* snarf */
				tok = strtok_r( NULL, ".;:/", &strtok_p );
			}
			else
			{
				fprintf( stderr, "lookup of host %s failed\n", tok );
				return 0;
			}
		}
		else        /* user entered xxx.xxx.xxx.xxx rather than name */
		{
			if( (tok = strrchr( dstr, ':' )) || (tok = strrchr( dstr, ';' )) || (tok = strrchr( dstr, '.' )) )  /* we assume address:port */
			{	
				*tok = 0;				/* end of string for address */
				tok++;					/* leave pointing at the port */
			}
			addr->sin_addr.s_addr = inet_addr( dstr );
		}
	
		if( tok )                 /* caller supplied a port or service name */
		{
			if( !isdigit( *tok ) && *tok != '-' )   /* get port or convert service name to port */
			{
				proto = strtok_r( NULL, ".;:", &strtok_p );   /* point at tcp/udp if there */
				if( ! proto )
					proto = "tcp";
				if( (sip = getservbyname( tok, proto )) != NULL )
				{
					addr->sin_port = sip->s_port;            /* save the port number */
					addr->sin_family = AF_INET;                 /* set family type */
				}
				else
				{
					fprintf( stderr, "lookup of port for %s (%s) failed\n", tok, proto ); 
					return 0;
				}
			}
			else
			{
				addr->sin_port = htons( atoi( tok ) );    /* save the port number */
				addr->sin_family = AF_INET;                 /* set family type */
			}
		}
		else
		{
			addr->sin_port = (unsigned) 0;    /* this will fail, but not core dump */
			addr->sin_family = AF_INET;                 /* set family type */
		}
		ng_free( dstr );                               /* release our work buffer */
	}                                   /* end convert from dotted */

	return 1;                     
}                                      /* convert */

/* accept a service name and protocol (tcp/udp) and get the port number */
int ng_getport( char *name, char *proto )
{
	struct servent *sip;

	if( isdigit( *name ) )		/* assume something like 1960 passed in */
		return atoi( name );
	else
        	if( (sip = getservbyname( name, proto )) != NULL )
          		return  ntohs( sip->s_port );
		else
			return -1;
}

#include "ng_testport.c"		/* include from here so that ng_serv can include for non-ast version */

/* ---------- SCD: Self Contained Documentation ------------------- */
#ifdef NEVER_DEFINED
&scd_start
&doc_title(ng_netif:Network Interface Support For Ningaui)

&space
&synop
	#include	<sys/socket.h> 
&break	#include	<netinet/in.h>
&break	#include	<netdb.h>    
&space
&break	char   *ng_getbroadcast( int fd, char *interface )
&break	int    ng_getport( char *name, char *proto )
&space
&break	int    ng_open_netudp( char *net, int port)
&break	int    ng_openudp( int port )
&break	void   ng_closeudp( int fd )
&space
&break	int    ng_readudp( int fd, char *buf, int len, char *from, int delay )
&break	int    ng_writeudp( int fd, char *to, char *buf, int len )
&break	int    ng_write_a_udp( int fd, struct sockaddr_in *to, char *buf, int len )
&space
&break	char   *ng_addr2dot( sockaddr_in * addr )
&break	char   *ng_ip2name( char *ip_addr_string )
&break	struct sockaddr_in *ng_dot2addr( char *src ) 
&space
	int	ng_testport( int fd, int delay )
&space
&break  int	ng_mcast_join( int fd, char *group, u_int32_t iface )
&break  int	ng_mcast_leave( int fd, char *group, u_int32_t iface )

&space
&desc	
	The routines contained within &lit(ng_netif.c) provide a simple
	interface to read and write UDP messages, and to convert
	&ital(dotted decimal) IP addresses to and from the 
	&lit(sockaddr_in) structure used by the underlying TCP and UDP 
	interface. The ningaui socket interface library (libng_si)
	provides a more complex callback environment that allows an 
	application programme to easily manage multiple TCP/IP sessions
	concurrently with UDP/IP traffic. These routines should be used
	when a single remote partner is all the application needs to 
	communciate with.
	
&space
	&ital(Ng_getport) accepts the service name and protocol type
	(UDP or TCP) and returns the associated port number for that
	application. If the lookup of the service name fails, then 
	a -1 is returned. The name may be the ASCII representation
	of the number (i.e. 1960) and if it is it is converted to 
	an integer. The port number returned is converted &stres(to)
	network byte order for the caller.

&space	&ital(Ng_openudp) Creates a UDP/IP socket for the caller 
	to send and receive UDP messages over. The caller must 
	supply the port number to open if a specific port is 
	desired. If any UDP port number can be allocated, then 
	a port number of -1 should be passed in. The file 
	descriptor for the socket is returned to the caller. 
	The port supplied is bound to any network so that 
	messages from any network destined for the supplied 
	port will be available to the application.

&space
	&ital(Ng_open_netudp) has the same function as does &lit(ng_openudp)
	excep the user must supply the network address that the 
	port is to be bound to.  UDP messages will be available 
	to the applicaiton only when received on the indicated 
	network interface. Supplying a network string of "00.00.00.00"
	is the same as invoking &lit(ng_openudp) to listen on 
	any interface currently available to the host.

&space
	&ital(Ng_closeudp) closes the file descriptor passed in. 

&space
	&ital(Ng_readudp) reads a message from a UDP port waiting 
	up to &ital(delay) seconds for a message to arrive. 
	If the caller desires the read call to block, then a delay
	of &stress(-1) should be passed. A delay of zero can be used
	to poll for data and will return immediatly to the caller 
	if nothing is available to read. 
	The message is placed into the users buffer pointed to by &ital(buf). 
	At a maximum, &ital(ng_readudp) will place &ital(size) bytes into 
	the buffer. The number of bytes placed into &ital(buf) is 
	returned by the routine. If the caller supplies &ital(from)
	the &ital(dotted decimal) IP address of the sender (including 
	the port number) is placed into the buffer pointed to by 
	&ital(from). If this information is not desired by the caller
	then &ital(from) should be &lit(NULL).

&space
	&ital(Ng_writeudp) writes a buffer to the UDP socket defined
	by the file descriptor &ital(fd) that is passed in. The 
	buffer (&ital(buf)) is written as is and is assumed to 
	contain &ital(len) bytes. The message is written to 
	the IP addres that is contained in the string pointed to 
	by &ital(to). The address &stress(must) contain a 
	port number or servant name, and may be in one of the previously 
	described formats.
&space
	&ital(Ng_write_a_udp) functions identically to &ital(ng_writeudp)
	except that it accepts the destination address as a &lit(sockaddr_in) 
	structure rather than a dotted decimal character string that 
	must be converted. For programmes that are sending a number of 
	messages to the same destination this routine can be more effecient. 


&space
	&ital(Ng_dot2addr) converts a dotted decimal string pointed to by 
	&ital(src) into a &lit(socaddr_in) structure. 
	The pointer to the new structure is returned. The caller is 
	responsible for disposing of the structure (using &lit(free))
	when it is no longer needed.
	The dotted decimal string may be one of the following formats:
&space
&beglitb
	ddd.ddd.ddd.ddd[{:|;}port]
	ddd.ddd.ddd.ddd[{:|;}service-name]
	host-name[{:|;}port]
	host-name[{:|;}service-name[/type]]
&endlitb
&space
	Where:
&begterms
&term	ddd : Is a one to three character decimal number (0-255).
&term	port : Is the port number. If not supplied a port number of zero 
	is placed into the structure. (Note that a port of zero will 
	likely fail any send/connection attempt, and the caller should 
	add the correct address to the structure before using it.)
&term	host-name : Is the name of the host to be looked up using the 
	&lit(gethostbyname()) system call (e.g. scooter.ohiou.edu). 
&term	service-name : Is the name of the service as listed in 
	&lit(/etc/services) that can be converted into a port number by 
	using the &lit(getservbyname()) system call. The service type 
	can optionally be appended to the name (&lit(/type)) and if 
	missing &lit(tcp) will be assumed.
&endterms

&space
	&ital(Ng_addr2dot) converts information in a &lit(sockaddr_in)
	structure to a &lit(NULL) terminated string that is the 
	&ital(dotted decimal) representation of the IP address contained
	in the structure. The function expects that the &ital(src) 
	parameter points to the &lit(sockaddr_in) structure and 
	returns a pointer to a dynamically allocated buffer containing
	the string. The caller is responsible for &lit(free)ing the 
	buffer when it is no longer needed. A &lit(NULL) pointer is 
	returned if an error occurs and the string cannot be generated.

&space
	&ital(Ng_ip2string) converts an string containing an IP address
	into a host name.  If the name would have been "localhost," 
	the current system name is returned. 


&space
	&ital(ng_getbroadcast) will return a NULL terminated character 
	string containing the address that can be used to send broadcast
	messages on the network attached to the interface
	&ital(ifname). 
	&ital(Ifname) is the name of the interface that should be 
	queried for its broadcast address (e.g. "ether0").
	The caller must concatinate a port number to this address before passing
	it to &ital(ng_writeudp), or any of the other routines 
	described here that accept an address as a character string,
	and because of this, the address returned has a trailing seperator
	character.
	It should be noted that not all 
	interfaces support the broadcast concept; if the interface
	does not support broadcast messages then a NULL pointer is 
	returned. 

&examp	The following example illustrates several calls to the
	address conversion routines.
&space
&beglitb
	addr = ng_dot2addr( "192.185.4.3:1960" );
	addr = ng_dot2addr( "192.185.4.3:bigears" );
	addr = ng_dot2addr( "192.185.4.3:bigears/ucp" );
	addr = ng_dot2addr( "bigbrother:bigears/ucp" );
	addr = ng_dot2addr( "bigbrother.listening.com:bigears/ucp" );
&endlitb
&space
	The next example is a code snipit that illustrates how 
	to open a UDP port, wait for a message and then echo 
	the message back to the sender.
&space
&beglitb
	int nfd;
	int len;
	char buf[1024];
	char from[100];

	len = ng_readudp( nfd, buf, sizeof( buf ), from, -1 );
	if( len )
		ng_writeudp( nfd, from, buf, len );
&endlitb

&space
	&ital(Ng_testport) will wait for up to &ital(delay) seconds
	and return &lit(1) if data is ready to be read from the 
	file descriptor &ital(fd). If after &ital(delay) seconds has 
	elapsed, and no data is yet ready to be read, &ital(ng_testport)
	returns &lit(0). 
	If there is an error, other than &lit(EINTR), &lit(-1) is returned
	to the caller.
	If &ital(delay) is set to &lit(-1), &ital(ng_testport) will 
	block completly until there is data to be read on the file 
	descriptor or there is an error.  
&space
	If an application needs to wait for data on 
	multiple file descriptors, it should implement a version of 
	this routine that makes &stress(one) &lit(select()) system 
	call for all file descriptors, and should &stress(not) 
	make several calls to this funciton.

&sapce
	&ital(Ng_mcast_join) and &ital(ng_mcast_leave) allow a process to 
	either join or leave a multi cast group.  The parameters for each 
	of these functions are the same and are:
&space
&begterms
&term	fd : A file descriptor that has been returned by the &lit(ng_openudp)
	or &lit(ng_open_netudp) function. 
&term	group : The multicast IP address of the group that is to be joined, or 
	left.
&term	iface : The interface name that is to be used. Typically this is the 
	contant &lit(INADDR_ANY) which indicates that any interface will do. 
&endterms

&see	socket(2), getsrvbyname(3), select(2), ng_serv(), ng_rx()
&space
&mods
&owner(Scott Daniels)
&lgterms
&term	27 Mar 2001 (sd) : Original Code
&term	09 Oct 2001 (sd) : Added testport() function to support select 
	orineted waiting by the ng_serv/ng_rx funciton set.
&term	18 Apr 2003 (sd) : To ensure no trailing blanks after interface name in broadcast.
&term	13 Sep 2003 (sd) : Removed some dependancies on the convert function and added 
	the open_netudp function. Added multicast support.
&term	03 Oct 2003 (sd) : Corrected address conversion to allow port to be separated
	by a dot (.).
&term	03 Aug 2004 (sd) : Changed malloc to ng_malloc.
&term	23 Aug 2004 (sd) : Updated comments with mcast join
&term	29 Sep 2004 (sd) : ng_testport now included from its own file to support the 
	non-ast lib.
&term	31 Jul 2005 (sd) : Changes to prevent warnings from gcc compiler.
&term	30 Aug 2006 (sd) : Better doc in malloc call to id this module when debugging.
			Converted strdup() calls to ng_strdup(). 
&term	01 Sep 2006 (sd) : Fixed coredump possibility in dot2addr().
&term	25 Jan 2008 (sd) : Corrected man page.
&term	25 Apr 2008 (sd) : Added ip2name() function. 
&endterms

&scd_end
#endif
