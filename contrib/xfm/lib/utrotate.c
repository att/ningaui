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
******************************************************************************
*
* Mnemonic: UTrotate
* Abstract: This routine will calculate the coordinates of a point that has
*           been rotated n degrees around a center point.
* Parms:    x, y - Coordinates of the point to rotate
*           rx, ry - Pointers to variables to return rotated coords in
*           cx, cy - Coordinates of the point that is the center of rotation
*           angle  - Number of degrees (positive = counter clockwise) to rotate
* Returns:  The coordinates of the rotated point.
* Date:     5 September 1991
* Author:   E. Scott Daniels
*
*******************************************************************************
*/
#include <math.h>
#include <stdlib.h>

#define DEG_TO_RAD    0.0174   /* number of radians in a degree */

void UTrotate( double x, double y, double *rx, double *ry, double cx, double cy,
          int angle )
{
 float rad;         /* number of radians to rotate */
 float radius;      /* radius of rotation */
 float newang;      /* rotated angle in radians */
 float a;
 float b;           /* vars to calculate the radius */

 rad = angle * DEG_TO_RAD;   /* convert degrees to radians */

 a = x - cx ;       /* adjust for center point of rotaion offset from 0,0 */
 b = y - cy ;       

 if( a == 0  &&  b == 0 )   /* absolutely no rotation */
  {
   *rx = x;
   *ry = y;
   return;    /* just pass them back what they had and go */
  }
                   /* calculate the radius of the rotation to this point */
 radius = sqrtf( ((a * a) + (b * b)) );  /* the old a squared+b squared...*/ 

 if( b < 0 && a >= 0 )                       /* if in quad 4 then */
  newang = rad + asin( b / radius );        /* use asin to calc angle */
 else
   if( b >= 0 )                              /* if in quads 1 or 2 use acos */
    newang = rad + acos( a / radius);      
   else                                     /* quad 3 and tangent is used */
    newang = rad + 3.1459 + atan( b / a );
 

   *rx = (cosf( newang ) * radius) + cx;        /* rotate the point */
   *ry = (sinf( newang ) * radius) + cy;

#ifdef KEEP
   printf( "\n UTrotate: (%f,%f) (%f,%f), \n     a= %f n= %f radius= %f ", 
            x, y, *rx, *ry, rad, newang, radius );
#endif

 return;      /* return number of points in the polygons */
}                    /* UTrotate */

