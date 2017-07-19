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


#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sfio.h>

/* convert a list of filenames into filesystem resource names for woomera
	/ningui/fs00/00/01/foo --> FS00
	/home/user/ng_root/fs02/00/00/foo --> FS00
	/fs/foo/bar/fs03/00/02/barfoo --> FS03
*/
static char *s_fsr( char **flist, int nflist )
{
	char	list[2000];		/* 400 bytes should be all we use */
	char	buf[20];		/* plenty big enough for FSxx things */
	int	hits[100];		/* track up to 100 file systems */
	int	i;
	int	f;
	int	max = -1;
	char	*file;
	char	*tok;

	memset( hits, 0, sizeof( hits ) );
	for( i = 0; i < nflist; i++ )
	{
		if( (file = flist[i]) != NULL )
		{
			tok = file;
			while( (tok = strstr( tok, "/fs" )) )
			{
 				if( isdigit( *(tok+3) ) )
				{	
					if( (f = atoi( tok+3 )) > max )
						max = f;
					if( f < 100 )
						hits[f] = 1;
					break;
				}

				tok++;
			}
		}
	}

	list[0] = 0;
	for( i = 0; i <= max; i++ )
	{
		if( hits[i] )
		{
			sfsprintf( buf, sizeof( buf ), "FS%02d%s", i, (i == max ? "" : " ") );
			strcat( list, buf );
		}
	}
	
	return strdup( list );
}

#ifdef SELF_TEST
main( )
{
	char *list = 0;
	char	*names[30];

	
        names[0] = "/ningaui/fs06/00/23/pinkpages.b002.cpt";
        names[1] = "/FS/smaug/ningaui/data/ng_root/fs00/00/04/pinkpages.b002.cpt";
        names[2] = "/FS/smaug/ningaui/data/ng_root/fs00/00/06/pinkpages.b002.cpt";
        names[3] = "/FS/smaug/ningaui/data/ng_root/fs00/00/02/pinkpages.b002.cpt";
        names[4] = "/fs/ningaui2/ng_root/fs10/00/16/pinkpages.b002.cpt";
        names[5] = "/FS/smaug/ningaui/data/ng_root/fs00/00/21/pinkpages.b002.cpt";
        names[6] = "/FS/smaug/ningaui/data/ng_root/fs00/00/23/pinkpages.b002.cpt";
        names[7] = "/FS/smaug/ningaui/data/ng_root/fs00/00/22/pinkpages.b002.cpt";
        names[8] = "/ningaui/fs05/00/22/pinkpages.b002.cpt";
        names[9] = "/ningaui/fs03/00/04/pinkpages.b002.cpt";
        names[10] = "/ningaui/xs02/00/15/pinkpages.b002.cpt";
        names[11] = "/ningaui/fs05/00/17/pinkpages.b002.cpt";

	list = fsr( names, 11 );
	printf( "list=(%s)\n", list );
	
}
#endif


