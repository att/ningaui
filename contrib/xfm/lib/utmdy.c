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
*  Mnemonic: UTmdy
*  Abstract: This routine will return to the caller the current month
*            day and year in the integers pointed to by the parameters
*            passed in.  MT-SAFE.
*  Parms:    m - Pointer to integer for the month
*            d - Pointer to the integer for the day
*            y - Pointer to the integer for the year
*  Returns:  Nothing (void) *m will be assigned 0 in the event of an error.
*  Date:     26 December 1996
*  Author:   E. Scott Daniels
*
**************************************************************************
*/
#include <sys/types.h>
#include <time.h>
#include <stdlib.h>

void UTmdy( int *m, int *d, int *y )
{
 time_t t;               /* pointer returned by clock, filled in by time */
 struct tm *cur_time;         /* structure for localtime call to fill in */

 time( &t );            /* get seconds past midnight on magic day */
 cur_time = localtime( &t );  /* get current time/day */

 *d = cur_time->tm_mday;         /* fill in caller's variables */
 *m = cur_time->tm_mon + 1;      /* make month 1 based */
 *y = cur_time->tm_year;         /* years since 1900 */

}                                 /* UTmdy */
