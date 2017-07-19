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


#ifndef __ut_h_
#define __ut_h_


/* --------- symtab ---------------- */
#define UT_FL_NOCOPY 0x00          /* use user pointer */
#define UT_FL_COPY 0x01            /* make a copy of the string data */
#define UT_FL_FREE 0x02            /* free val when deleting */


typedef struct Sym_ele
{
	struct Sym_ele *next;          /* pointer at next element in list */
	struct Sym_ele *prev;          /* larger table, easier deletes */
	unsigned char *name;           /* symbol name */
	void *val;                     /* user data associated with name */
	unsigned long mcount;          /* modificaitons to value */
	unsigned long rcount;          /* references to symbol */
	unsigned int flags; 
	unsigned int class;		/* helps divide things up and allows for duplicate names */
} Sym_ele;

typedef struct Sym_tab {
	Sym_ele **symlist;			/* pointer to list of element pointerss */
	long	inhabitants;             	/* number of active residents */
	long	deaths;                 	/* number of deletes */
	long	size;	
} Sym_tab;

/* -------------- graphics (image.c, image_comb.c, load_*.c) ---------------------------- */

#define UT_SQUISH_HEIGHT	1		/* get_compressed_image -- compress in height direction */
#define UT_SQUISH_WIDTH		0		/* squish width by factor */
#define UT_IGNORE_BELOW		1		/* ignore values below the threshold provided */
#define UT_IGNORE_ABOVE		2		/* ignore values over the threshold */

#define UT_FILTER_ABOVE		0		/* filtering options -- nix individual colour value if above threshold */
#define UT_FILTER_BELOW		1		/* filter individual colour value if below */
#define UT_FILTER_BETWEEN 	2		/* filter individual colour value if between */
#define UT_FILTER_ALL_ABOVE	3		/* same as above, but only if all values are above the various thresholds */
#define UT_FILTER_ALL_BELOW	4		/* same as below, but only if all values are below */
#define UT_FILTER_BLURR 	5


#define UT_VERTICAL		1		/* must not use 0 for horz or vert! */
#define UT_HORIZONTAL		2

typedef struct Image {		/* generic image information */
	void	*ximage;	/* probably XImage, but void so we dont have to include X.h; something for x to display -- from XI_newimage */
	char 	*pxmap;		/* map from png/jpg load */
	int	height;
	int	width;
	int	depth;
	int	bpp;		/* bytes per pix */
} Image_t;


/* -------------- prototypes ----------------------- */
char *UTgetevar( char **elist, char *str );
void UTtimes( int *m, int *d, int *y );
void UTrotate( double x, double y, double *rx, double *ry, double cx, double cy, int angle );

/* ------------- image stuff --------------------- */
Image_t *UTcompress_image( Image_t *ip, int factor, int direction );
void 	UTfilter_image( Image_t *ip, int rp, int gp, int bp, int filter_greater ); /* filter out the r/g/b if value is > or < than r pct, gpct... */
Image_t *UTget_image( char *fname, int max_w, int max_h );
Image_t *UTget_compressed_image( char *fname, int max_h, int max_w, int factor, int direction );
void 	UTimage2bnw( Image_t *ip );
char 	*UTload_png( char *fname, int *uheight, int *uwidth, int *udepth, int *ubpp );
char 	*UTload_jpg( char *fname, int *uheight, int *uwidth, int *udepth, int *ubpp );
void 	UTmirror_image( Image_t *ip, int axis );
void 	UTnegate_image( Image_t *ip );
void UTnormalise_image( Image_t *ip );
Image_t *UTnew_image( int height, int width, int bpp );
char 	*UTsave_jpg( char *fname, unsigned char *pxmap, int height, int width, int bpp, int quality );
void 	UTshade_image( Image_t *ip, int rp, int bp, int gp );
Image_t *UTthumbnail_image( Image_t *ip, int width, int height );
/*
	image-comb combines the bottom and top images applying the transparency value to 
	the top image. The transparency is 0-255 with 255 being opaque and 0 being fully
	transparent.
	The top image is clipped using the bottom image dimensions. If startx is < 0, 
	then the top image is palaced automatically in the lower right corner of the 
	bottom image. 
*/
void UTimage_comb( Image_t *ibot, Image_t *itop, int startx, int starty, int transp );

/* -----------  utmalloc.c -------------------------- */
void *UTmalloc( int i );
void *UTrealloc( void *, int i );
char *UTstrdup( char *str );
void UTfree( void *p );

/* -------------- time.c --------------------------- */
char *cvt_odate( char *ostr );			/* convert oracle style time into string */
unsigned long UTtime_ss2ts( char *d, char *t );    /* convert date string (d) and time string (s) into integer date is yyyy/mm/dd or mm/dd/yyyy */
unsigned long UTtime_s2ts( char *s );		/* convert a sigle string containing date and time into integer */
unsigned long UTtime_now();			/* current time */
int *UTtime_ts2i( unsigned long ts );  		/* convert timestamp into an array integers (m,d,y,h,min,s) */
char *UTtime_ts2dow( unsigned long ts );	/* convert timestamp into day of week string */
char *UTtime_ts2s( unsigned long ts );		/* convert timestamp into string */



/* ------------ symtab ----------------------------- */
extern void sym_clear( Sym_tab *s );
extern void sym_dump( Sym_tab *s );
extern Sym_tab *sym_alloc( int size );
extern void sym_del( Sym_tab *s, unsigned char *name, unsigned int class );
extern void *sym_get( Sym_tab *s, unsigned char *name, unsigned int class );
extern int sym_put( Sym_tab *s, unsigned char *name, unsigned int class, void *val );
extern int sym_map( Sym_tab *s, unsigned char *name, unsigned int class, void *val );
extern void sym_stats( Sym_tab *s, int level );
void sym_foreach_class( Sym_tab *st, unsigned int class, void (* user_fun)(), void *user_data );

#endif
