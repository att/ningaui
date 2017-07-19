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
*************************************************************************
*
*  Mnemonic: UTtimes
*  Abstract: This routine will return to the caller the current time in 
*            hour min sec format. 
*  Parms:    h - Pointer to integer for the hour
*            m - Pointer to the integer for the min
*            s - Pointer to the integer for the sec
*  Returns:  Nothing (void) *h will be assigned -1 in the event of an error.
*  Date:     20 January 1997
*  Author:   E. Scott Daniels
*
**************************************************************************
*/
#include <sys/types.h>
#include <time.h>
#include <stdlib.h>

void UTtimes( int *h, int *m, int *s )
{
 time_t t;               /* pointer returned by clock, filled in by time */
 struct tm *cur_time;         /* structure for localtime call to fill in */

 *h = -1;                     /* default to error */

 time( &t );            /* get seconds past midnight on magic day */
 cur_time = localtime( &t );  /* get current time/day */

 *h = cur_time->tm_hour;
 *m = cur_time->tm_min;
 *s = cur_time->tm_sec;
}                     /* UTtimes */

void UThms( int *h, int *m, int *s )
{
 UTtimes( h, m, s );
}
