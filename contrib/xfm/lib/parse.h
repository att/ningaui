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
/* constants needed by parse.c or functions that call vreplace */

#define VRF_CONSTBUF	0		/* dummy flag - return string in static buffer */
#define VRF_NEWBUF	0x01		/* create a new buffer */
#define VRF_NORECURSE	0x02		/* no recursive expansion */

#define F_PARMS	0x01			/* internal flag - expand parms first */

char *vreplace( Sym_tab *st, int namespace, char *sbuf, char vsym, char esym, char psym, int flags );
