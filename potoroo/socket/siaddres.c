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

/* DEPRECATED -
	use ng_dot2addr and ng_addr2dot from the ng_network lib set
*/
/* X
**************************************************************************
*
*  Mnemonic: ng_siaddress
*  Abstract: This routine will convert a sockaddr_in structure to a
*            dotted decimal address, or visa versa.
*            If type == AC_TOADDR the src string may be: 
*            xxx.xxx.xxx.xxx.portnumber or host-name.portnumber
*            xxx.xxx.xxx.xxx.service[.protocol] or hostname;service[;protocol]
*            if protocol is not supplied then tcp is assumed.
*            hostname may be something like godzilla.moviemania.com
*  Parms:    src - Pointer to source buffer
*            dest- Pointer to dest buffer
*            type- Type of conversion
*  Returns:  Nothing.
*  Date:     19 January 1995
*  Author:   E. Scott Daniels
*
*  Modified: 22 Mar 1995 - To add support for ipx addresses.
*
*  CAUTION: The netdb.h header file is a bit off when it sets up the 
*           hostent structure. It claims that h_addr_list is a pointer 
*           to character pointers, but it is really a pointer to a list 
*           of pointers to integers!!!
*       
***************************************************************************
*/
#include "sisetup.h"      /* get necessary defs and other stuff */
#include <netdb.h>

void ng_siaddress( void *src, void *dest, int type )
{
 struct sockaddr_in *addr;       /* pointer to the address */
 struct ipxaddr_s *ipxaddr;      /* pointer to ipx address */
 char *dstr;                     /* pointer to decimal string */
 unsigned char *num;             /* pointer at the address number */
 char wbuf[80];                  /* work buffer */
 int i;                          /* loop index */
 char *tok;                      /* token pointer into dotted list */
 char *proto;                    /* pointer to protocol string*/
 char *strtok_p;		/* work pointer for calls to strtok_r */
 struct hostent *hip;            /* host info from name server */
 struct servent *sip;            /* server info pointer */


 if( type == AC_TODOT )          /* convert to dot string */
  {
   addr = (struct sockaddr_in *) src;
   dstr = (char *) dest;                     /* dest is decimal string */
   num = (char *) &addr->sin_addr.s_addr;    /* point at the long */
   *dstr = EOS;
   for( i = 0; i < 4; i++ )         /* convert each dig to string */
    {
     sprintf( wbuf, "%u.", *(num++) );
     strcat( dstr, wbuf );
    }
   sprintf( wbuf, "%u", htons( addr->sin_port ) );   /* convert port to character */
   strcat( dstr, wbuf );                     /* and add it to large str */
  }
 else
  if( type == AC_TOADDR )         /* from dotted decimal to address */
   {
    dstr = strdup( src );
    addr = (struct sockaddr_in *) dest;                   /* set pointers */
    /*tok = strtok_r( dstr, ".;: ", &strtok_p );     */         /*point at first token */
    addr->sin_addr.s_addr =  0;
    addr->sin_port = 0;              /* incase things fail */

    if( ! isdigit( *dstr ) )       /* assume hostname.port */
     {
      tok = strtok_r( dstr, ";: ", &strtok_p );              /*point at first token */
      if( (hip = gethostbyname( tok )) != NULL )   /* look up host */
       {
        addr = (struct sockaddr_in *) dest;         /* set pointers */
        addr->sin_addr.s_addr =  *((int *) (hip->h_addr_list[0]));  /* snarf */
        tok = strtok_r( NULL, ".;:", &strtok_p );
       }
      else
        fprintf( stderr, "looup of host %s failed\n", tok );
     }
    else        /* user entered xxx.xxx.xxx.xxx rather than name */
     {
      tok = strtok_r( dstr, ".;: ", &strtok_p );              /*point at first token */
      addr->sin_addr.s_addr = 0;                /* start off as 0 */
      num = (char *) &addr->sin_addr.s_addr;    /* point at the long */
      for( i = 0; tok != NULL && i < 4; i++ )
       {
        *(num++) = (unsigned char) atoi( tok );   /* add next digit on */
        tok = strtok_r( NULL, ".; ", &strtok_p );              /* at next tok in list */
       }                                          /* end for - tok @ port # */
     }

    if( tok )                 /* caller remembered a port to */
     {
      if( !isdigit( *tok ) )   /* get port or convert service name to port */
       {
        proto = strtok_r( NULL, ".;:", &strtok_p );     /* point at tcp/udp if there */
        if( ! proto )
         proto = "tcp";
        if( (sip = getservbyname( tok, proto )) != NULL )
         {
          addr->sin_port = sip->s_port;            /* save the port number */
          addr->sin_family = AF_INET;                 /* set family type */
         }
        else
         fprintf( stderr, "looup of port for %s (%s) failed\n", tok, proto );
       }
      else
       {
        addr->sin_port = htons( atoi( tok ) );    /* save the port number */
        addr->sin_family = AF_INET;                 /* set family type */
       }
     }
    else
     {
      addr->sin_port = (unsigned) 0;         /* this will fail, but not dump */
      addr->sin_family = AF_INET;                 /* set family type */
     }
 
    ng_free( dstr );                               /* release our work buffer */
   }                                   /* end convert from dotted */
}    /* ng_siaddress */

