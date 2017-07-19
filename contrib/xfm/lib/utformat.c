/*
* ---------------------------------------------------------------------------
* This source code is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* If this code is modified and/or redistributed, please retain this
* header, as well as any other 'flower-box' headers, that are
* contained in the code in order to give credit where credit is due.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* Please use this URL to review the GNU General Public License:
* http://www.gnu.org/licenses/gpl.txt
* ---------------------------------------------------------------------------
*/
/*
***************************************************************************
*
*  Mnemonic: UTformat
*  Abstract: This routine is responsible for taking an input string or list
*            of tokens and reformatting them placing the tokens into the
*            output buffer as directed by the information in the control
*            buffer. The control buffer can contain literal information or
*            token representation variables ($n) allowing the tokens to
*            be placed into the output buffer in any order. Each token variable
*            may be preceeded by a field size specifier (#n) indicating how
*            much space should be provided for the token in the output field
*            (filling with spaces if the token is smaller than the field).
*            The carrot (^) is the escape symbol which allows dollar signs ($)
*            to be placed in the output string and to allow numbers to be
*            placed directly following tokens with out spaces ($2^103).
*            An ampersand (&) can be used to place the indicated token
*            and all that follow into the output buffer.
*  Examples:
*            input string = "Now is the time for the aardvark to reign."
*            control string="  It $2 $3 $7^'s $4 &8"
*            resulting output string =
*               "  It is the aardvark's time to reign."
*  Parms:    inbuf - pointer to the input string of tokens; NULL if inlist
*                    is supplied.
*            inlist- List of input tokens already parsed
*            outbuf- pointer to the destination area
*            control-pointer to the buffer that controls format into output
*            delim  -pointer to delimieter(s) seperating input tokens
*            escsym - The escape character
*  Returns:  Nothing.
*  Date:     27 December 1993
*  Author:   E. Scott Daniels
*
*  Modified: 10 Feb 1995 - To free toks array if we allocated it
*            23 Sep 2000 - To impose sanity on field sizes. Thus #rrggbb 
*                          values do not need to be escaped in macros
***************************************************************************
*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define EOS (char) 0

/* copy from sub to out replacing escaped delimiters with their char */
int copybuf( char *in, char *out, char escsym )
 {
  int k = 0;

  while( *in )
   {
    if( *in == escsym && *(in+1) & 0x80  )       /* escaped delimiter */
     in++;

    *out++ = *in++ & 0x7f;                       /* ensure normal char */
    k++;
   }

  return( k );                /* send number of characters inserted */
 }


void UTformat( char *inbuf, char **inlist, char *outbuf,
               char *control, char *delim, char escsym)
{
 int fieldsize = 0; /* minimum field size to display a token in */
 int i;             /* loop indexes - token vars */
 int j;             /* index intro control buffer */
 int k;             /* index into output buffer */
 int idx;           /* index into toks array as specfied by $n in control */
 char **toks;       /* pointers to tokens to process */
 char *tok;         /* pointer at current tok from input buffer */
 char *sub;         /* pointer at token to substitute for the $n param */
 int count;         /* number of tokens to prevent writing bad data */
 int freeflag = 0;  /* flag indicating whether tok array is freed at end */


 if( inbuf != NULL )   /* if a string passed in for us to parse */
  {
   freeflag = 1;                             /* we must free it at the end */
   toks = (char **) malloc( (unsigned) (256 * sizeof (char *)) );

   /*tok = strtok( inbuf, delim ); */ /* get first token */

   tok = inbuf;
   i = 0;
   while( *tok && i < 255 )
    {
     toks[i] = tok;
     for( ; *tok && !strchr( delim, *tok ); tok++ )
      if( *tok == escsym && strchr( delim, *(tok+1)) )
       {
        tok++;
        *tok |= 0x80;
       }
     if( *tok )
      *tok++ = EOS;
     i++;
    }
   toks[i] = NULL;   /* last one in the list is NULL */

#ifdef KEEP
   /* toks[0] = tok; */                /* and put it in the buffer */
   for( i = 1; (tok = strtok( NULL, delim )) != NULL && i < 29; i++ )
    toks[i] = tok;   /* capture pointers to each token in input buffer */
   toks[i] = NULL;   /* last one in the list is NULL */
#endif
   count = i;
  }                  /* end if string passed in to parse */
 else                /* a list of tokens passed in to use */
  {
   for( count = 0; inlist[count] != NULL;  count++ );
   toks = inlist;     /* point at the list for the rest to use */
  }

 for( j = 0, k = 0; control[j] != EOS; j++ )
  {
   if( control[j] == '#' )   /* field size specified */
    {
     fieldsize = atoi( &control[j+1] );  /* save it for next $ in control str*/
     if( fieldsize <=0 || fieldsize >= 80 )    /* impose sanity if needed */
      fieldsize = 0;
     else
      for( j++; control[j+1] != EOS && control[j+1] >= '0' &&
               control[j+1] <= '9'; j++ );   /* skip #parm in control */
    }
   else
   if( control[j] == '$' )   /* substitute an input token? */
    {
     idx = atoi( &control[j+1] ) - 1;  /* get user's number & make 0 based */
     for( j++; control[j+1] != EOS && control[j+1] >= '0' &&
               control[j+1] <= '9'; j++ );         /* skip $parm in control */

     sub = toks[idx];          /* point at string to place into output buf */
     if( sub == NULL || idx > count )         /* bad meat - just use blank */
      sub = " ";                                  /* just write in a blank */


     if( fieldsize > 0 )   /* previous # parm encountered in control? */
      {
       i = fieldsize - strlen( sub ); /* calc spaces to skip */
       for( ; i > 0; i-- )
        outbuf[k++] = ' ';   /* add lead blanks if we can */
      }

     k += copybuf( sub, &outbuf[k], escsym );           /* copy token in */

     if( fieldsize < 0 )         /* left justify # encountered previously */
      {
       i = abs( fieldsize ) - strlen( sub ); /* calc spaces to skip */
       for( ; i > 0; i-- )
        outbuf[k++] = ' ';   /* add trailing blanks if we can */
      }
     fieldsize = 0;    /* reset */
    }    /* end if $ */
   else
    if( control[j] == '~' )   /* print all tokens from the number on out */
     {
      idx = atoi( &control[j+1] ) - 1;  /* get user's number & make 0 based */
      for( j++; control[j+1] != EOS && control[j+1] >= '0' &&
                control[j+1] <= '9'; j++ );   /* skip $parm in control */
      while( (sub = toks[idx]) != NULL )      /* put in remaining toks */
       {
        k += copybuf( sub, &outbuf[k], escsym );
/*
        for( i = 0; sub[i] != EOS; i++ )
         outbuf[k++] = sub[i];  
*/
        idx++;
        outbuf[k++] = ' ';          /* seperate with a blank */
       }                            /* end while */
      k--;                          /* dont leave last trailing blank */
     }                              /* end if & */
    else
     if( control[j] == '^' &&     /* escape only utformat special characters */
         (control[j+1] == '~' || control[j+1] == '$' || control[j+1] == '#') )
      {
       ++j;                        /* skip escape symbol */
       outbuf[k++] = control[j];   /* then copy in the escaped character */
      }
     else
      outbuf[k++] = control[j];   /* just copy next character */
  }                               /* end for j,k */

 if( freeflag )
  free( toks );         /* free it if we allocated it */
 outbuf[k] = EOS;       /* terminate the buffer properly */
}                       /* UTformat */
