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
*****************************************************************************
*
* Mnemonic: afisetup.h
* Abstract: This file is included by all afi routines to make the
*           administration of header files and general compilation issues
*           easier. This file will include all needed .h files for each
*           routine.
* Author:   E. Scott Daniels
*
****************************************************************************
*/
#define _REENTRANT 1                 /* include _r prototypes */

#include <stdio.h>                   /* include from searched location */
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <memory.h>
#include <string.h>
/* #include <malloc.h>    deprecated in some new installations */

#include	"../lib/symtab.h"

#include "aficonst.h"     /* internal - private definitions- afi only */
#include "afidefs.h"      /* external - public definitions */
#include "afistruc.h"     /* structure definitions */

/* private protos */
void *AFInew( int type );
