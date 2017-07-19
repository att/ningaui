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
  Mnemonic: cte.c - Content Transport Encoding
  Abstract: This program will convert the named file (or stdin) to 
            the desired encoded format for transport via a mail environment.
  Date:     20 December 1997
  Author:   E. Scott Daniels
-------------SCD AT END OF FILE ------------------------------------------
*/

#include	<unistd.h>
#include	<string.h>
#include	<stdlib.h>
#include	<errno.h>

#include	<ningaui.h>

#include	"ng_cte.man"

static int doquote( Sfio_t *f );
static int base64( Sfio_t *f );

                            /* Types of encoding that we provide  */
#define QUOTED 0            /* -q (default) */
#define BASE64 1            /* -6 Not yet implemented */


#define MAX_CHRS 76         /* max characters on output line */

int cvtnl = 0;               /* 1 if we should add 0x0d to \n characters */

int main( int argc, char **argv )
{

 int status = 1;            /* exit status */
 int type=QUOTED;
 char *fname = "/dev/fd/0"; /* input file (default to stdin */
 Sfio_t *f;                   /* input handle */

 argv0 = argv[0];

 ARGBEGIN
  {
   case 'c': cvtnl++;        break;
   case 'q': type = QUOTED;  break;   
   case '6': type = BASE64;  break;
   default:
usage:
     sfprintf( sfstderr, "%s [-q | -6] [-c] [filename]\n", argv0 );
     exit( 1 );
     break;
  }
 ARGEND

if( argc > 0 )
 fname = argv[0];               /* get input name */


 if( (f = ng_sfopen( NULL, fname, "r" )) )
  {
   switch( type )  
    {
     case QUOTED:
        status = doquote( f );
        break;
 
     case BASE64:
        status = base64( f );
        break;
 
     default:
        sfprintf( sfstderr, "Internal cte error: unknown type: %d\n", type );
        break;
 
    }
   if( (status = sfclose( f ) ) )
	ng_bleat( 0, "error on write or close; %s: %s", fname, strerror( errno ) );
  }
 else
  sfprintf( sfstderr, "%s: cannot open: %s: %s\n", argv0, fname, strerror( errno ) );

 exit( status ); 
}

/*
-------------------------------------------------------------------------
  Mnemonic: doquote
  Abstract: This routine will read from f and encode it into 
            quoted-printable format. The encoding algorithm is based on 
            RFC-2045.
--------------------------------------------------------------------------
*/
                            /* RFC2045 specifies CRLF rather than single LF */
#define SOFTCR "=\x0d\x0a"  /* encoded end of line (soft crlf) */

static int doquote( Sfio_t *f )
{
 char ibuf[2048];
 char obuf[MAX_CHRS+2];
 char *iidx;                /* indexes into input and output buffers */
 char *oidx;
 int len;                   /* len of data in input buffer */


 oidx = obuf;

 sfprintf( sfstdout, "Content-Transfer-Encoding: quoted-printable\n\n" );

 while( (len = sfread( f, ibuf, 2048 )) > 0 )
  {
   for( iidx = ibuf; len; len-- )
    {
     if( oidx - obuf >= MAX_CHRS - 6 )                /* not enough room */
      {
       *oidx = 0;
       if( sfprintf( sfstdout, "%s%s", obuf, SOFTCR ) < 0 )
        {
         sfprintf(sfstderr, "%s: Error writing: %s\n", argv0, strerror( errno ));
         return( 1 );
        }

       oidx = obuf;
      }

                                        
     if( (*iidx >= 32 && *iidx <= 60)  ||  /* see if char can go unencoded */
         (*iidx >= 62 && *iidx <= 126) ||
         *iidx == 9 )                      /* ok cuz the way we do soft cr */
      *oidx++ = *iidx++;                   /* send it as it is */
     else
      {
       if( cvtnl && *iidx == '\n' )     /* need to cvt for spacewasting PC */
        {
         sprintf( oidx, "=0D" );       /* so add the extra dog */
         oidx += 3; 
        }

       *oidx++ = '=';                        /* escape character into buffer */
       sprintf( oidx, "%02X", (unsigned char) *iidx );  /* RFC says must be X*/
       oidx += 2;   
       iidx++;
      }
    }
  }

 *oidx = 0;
 if( sfprintf( sfstdout, "%s%s\n", obuf, SOFTCR ) < 0 ) /* send last */
  {
   sfprintf( sfstderr, "%s: Error writing: %s\n", argv0, strerror( errno ) );
   return( 1 );
  }

 return( ferror( f ) );
}

/*
-----------------------------------------------------------------------------
-
*  Mnemonic:  base64
*  Abstract:  This routine will read until the end of file f and convert it
*             into base64.  The converted buffer(s) are written to the
*             standard output device. The conversion is based on RFC2045.
*             NOTE: Base 64 is primarily for binary files as it will not 
*                   convert newlines into CRLFs as may be required by some 
*                   PC receiving mail processes. 
*  Parms:     f - Pointer to Sfio_t of input file
*  Returns:   0 if all ok, 1 if read/write errors occured
*  Date:      21 December 1997
*  Author:    E. Scott Daniels
-----------------------------------------------------------------------------
*/

static unsigned char encoded[64] = {  /* encoded bits */
'A','B','C','D','E','F','G','H',
'I','J','K','L','M','N','O','P',
'Q','R','S','T','U','V','W','X',
'Y','Z','a','b','c','d','e','f',
'g','h','i','j','k','l','m','n',
'o','p','q','r','s','t','u','v',
'w','x','y','z','0','1','2','3',
'4','5','6','7','8','9','+','/'
};
 

#define PAD_CHAR '='          /* encoded padding character */
#define READLEN 1020          /* must be a multiple of three */
#define NEW_LINE  "\x0d\n"    /* end of line string (RFC822) */
 
int base64( Sfio_t *f )
{
 char ibuf[READLEN+3];        /* must have room for padding at end */
 char  obuf[MAX_CHRS+5];      /* output buffer */
 char *iidx;                  /* input index */
 char *oidx;                  /* index into output buffer */
 int len;                     /* num bytes actually read */
 
 sfprintf( sfstdout, "Content-Transfer-Encoding: base64\n\n" );

 oidx = obuf;

 while( (len = sfread( f, ibuf, READLEN )) > 0 )
  {
   *(ibuf+len) = 0;
   *(ibuf+len+1) = 0;   /* ensure proper padding if short read at end of file*/
   iidx = ibuf;
 
   while( iidx < ibuf+len )
    {            /* do not iidx++ inside of [] because of side effects! */
     *oidx++ = encoded[(*iidx >> 2) & 0x3F];       
     *oidx++ = encoded[((*iidx & 0x03) << 4) + ((*(iidx+1) & 0xF0) >> 4)]; 
     iidx++;
     *oidx++ = iidx >= (ibuf + len) ? PAD_CHAR :
                   encoded[((*iidx & 0x0f)<< 2) + ((*(iidx+1) & 0xC0) >> 6)];
     iidx++;
     *oidx++ = iidx >= (ibuf + len) ? PAD_CHAR : encoded[*iidx & 0x3F];
     iidx++;
 
     if( oidx >= obuf + MAX_CHRS )        /* need to write output buffer */
      {
       *oidx = 0;                                   /* make a string */
       sfprintf( sfstdout, "%s%s", obuf, NEW_LINE );
       oidx = obuf;
      }
    }
  }

 *oidx = 0;                                   /* write last buffer */
 sfprintf( sfstdout, "%s%s", obuf, NEW_LINE );
 oidx = obuf;
 
 return( ferror( f ) + ferror( sfstdout ) );
}

/* --------------- SELF CONTAINED DOCUMENTATION -------------------------*/
#ifdef SCD    /* ensure compiler does not see this */
&scd_start
&doc_title(ng_cte:Content Transfer Encoder)

&space
&synop  ng_cte [-q | -6] [-c] [filename]

&space
&desc	&ital(Ng_cte) will encode the input file using either 
	quoted-printable or base64 encoding techniques.  The encoded
	text is written to the standard output device. The encoded 
	file may safely be mailed with little fear of the mailer(s)
	altering the format of the file.
&space
	Both encoding algorithms supported by this program are based 
	on the standard set by documents RFC2045 through RFC2048.

&space
&opts	&ital(Ng_cte) accepts the following options on the command 
	line.
&begterms
&term   -c : Add a carriage return character (0x0d) to each new line 
	encountered in the file. This may be necessary for some 
	PC based applications to properly handle the file. This option 
	applied only when encoding is being done using the quoted-printable
	algorithm.
&space
&term	-q : Use the quoted-printable algorithm. This is the default
	if no algorithm type option is specified.
&space
&term   -6 : Use the base64 algorithm to encode the input file. This 
	is a safe method of mailing binary files. If the file should
	have newlines (0x0a) converted to carriage return/linefeed 
	combinations, this conversion needs to be preformed on the 
	file &stress(before) the file is passed to &ital(ng_cte).
&endterms

&space
&parms	The user may optionally supply one parameter on the command line.
	This is the name of the input file that the program is to parse. 
	If the parameter is omitted, then the standard input is read 
	and encoded. 

&space
&exit	&ital(Ng_cte) will exit with a zero if all processing was successful.
	A non-zero return code is generated when an error is detected.

&errors The following error messages are written to the standard error 
	device.
&beglist
&item   &lit(Error writing:) &ital(system message) 
&break  The program received an error from the operating system when 
	trying to write to the standard output device. The system error 
	message is printed after the colon.
&space
&item   &lit(Cannot open:) &ital( filename: system message)
&break  The program could not open the named file for reading. The system 
	error message is printed following the filename.
&space
&item   &lit(Internal cte error: unknown type:) &ital(n)
&break  The type of encoding, internally represented by the value &ital(n),
	was not recognized. This indicates a potential programming error.
&space
&endlist

&space
&see    RFC2045

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	20 Dec 1997 (sd) : Original Code
&term	20 Apr 2001 (sd) : Conversion from Gecko.
&endterms
&scd_end
#endif
