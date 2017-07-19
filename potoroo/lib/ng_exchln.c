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
*--------------------------------------------------------------------------
*
*  Mnemonic: ng_exchln.c
*  Date:     2 September 1997
*  Author:   E. Scott Daniels
*
*---------------------------------------------------------------------------
*/
#include <unistd.h>
#include	<sfio.h>

int ng_exchln( char *l1, char *l2 )
{
 char r1[1024];             /* buffers for real names (files pointed to */
 char r2[1024];             /* by the symbolic links */
 int status = 1;            /* status of local processing for return */
 int len;                   /* length of info placed into r1 and r2 */


 if( (len = readlink( l1, r1, 1023 )) > 0 )   /* get what they point at now */
  {
   status++;
   r1[len] = '\000';         /* readlink does not return terminated string */

   if( (len = readlink( l2, r2, 1023 )) > 0 )
    {
     status++;
     r2[len] = '\000';

     if(  unlink( l1 ) == 0 )           /* remove the links */
      {
       status++;

       if( unlink( l2 ) == 0 )
        {  
         status++;

         if( symlink( r2, l1 ) == 0 )      /* repoint at what the other */
          {                                /* pointed @ when we were called */
           status++;

           if( symlink( r1, l2 ) == 0 )
            status = 0;                     /* all OK - inidicate good stat */
          }
        }
      }
    }
  }
 
 return( status ); 
}


/*
#--- SELF CONTAINED DOCUMENTATION SECTION -------------------------------
&scd_start
&doc_title(ng_exchln:Exchange values of two symbolic links)

&space
&synop	<include ningaui.h>
	int ng_exchln( char *linkname1, char *linkname2 );

&space
&desc	The &this function will exchange the two symbolic link
	names passed in such that the real file pointed to by linkname1 
	will be pointed to by linkname2, and linkname1 will point to 
	the file originally pointed to by linkname2. 


&space
&parms	The following describe the parameters required by the funciton.
&begterms
&term	linkname1 : A pointer to a null terminated string containing 
	the first symbolic link name.
&space
&term	linkname2 : A pointer to a null terminated string containing 
	the second symbolic link name.
&endterms

&space
&return	The function returns the value of 0 on success, and a value 
	of !0 on faiure.  If failure is indicated the global variable
	errno is set to indicate the reason for failure.  The errno 
	value will be as set by the system call that failed. 

&space
&errors	The return value indicates the area of processing that failed.
	The following values can be used to determine the soource of 
	the &ital(errno) value.
&space
&begterms
&term	1 : The link &ital(linkname1) could not be read.
&term	2 : The link &ital(linkname2) could not be read.
&term	3 : The link &ital(linkname1) could not be unlinked.
&term	4 : The link &ital(linkname2) could not be unlinked.
&term   5 : The link of &ital(linkname1) could not be reestablished.
&term   6 : The link of &ital(linkname2) could not be reestablished.
&endterms

&space
&examp

&space
&see 	readlink(2), symlink(2), unlink(2) 

&space
&mods
&owner(Scott Daniels)
&begterms
&term	02 Sep 1997 (sd) - Original code. 
&term	28 Mar 2001 (sd) - Converted from Gecko.
&endterms

&scd_end
*/
