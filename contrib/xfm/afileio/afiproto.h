/*
* =====================================================================================
* 
* 	This source code is free software; you can redistribute it and/or modify
* 	it under the terms of the GNU General Public License as published by
* 	the Free Software Foundation; either version 2 of the License, or        
* 	(at your option) any later version.          
* 	
* 	This program is distributed in the hope that it will be useful,
* 	but WITHOUT ANY WARRANTY; without even the implied warranty of
* 	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* 	GNU General Public License for more details.
* 	
* 	Please use this URL to review the GNU General Public License:
* 	http://www.gnu.org/licenses/gpl.txt             
* 
* ====================================================================================
* 
*/
/*
* ============================================================= $id_tag
* 
* 	This source code is free software; you can redistribute it and/or modify
* 	it under the terms of the GNU General Public License as published by
* 	the Free Software Foundation; either version 2 of the License, or        
* 	(at your option) any later version.          
* 	
* 	This program is distributed in the hope that it will be useful,
* 	but WITHOUT ANY WARRANTY; without even the implied warranty of
* 	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* 	GNU General Public License for more details.
* 	
* 	Please use this URL to review the GNU General Public License:
* 	http://www.gnu.org/licenses/gpl.txt             
* 
* ============================================================= $trail_tag
* 
*/
/*
* /*     
* _LINE_COMMENT_SYM ============================================================= $id_tag
* _LINE_COMMENT_SYM
* _LINE_COMMENT_SYM     This source code is free software; you can redistribute it and/or modify
* _LINE_COMMENT_SYM     it under the terms of the GNU General Public License as published by
* _LINE_COMMENT_SYM     the Free Software Foundation; either version 2 of the License, or        
* _LINE_COMMENT_SYM       (at your option) any later version.          
* _LINE_COMMENT_SYM   
* _LINE_COMMENT_SYM     This program is distributed in the hope that it will be useful,
* _LINE_COMMENT_SYM     but WITHOUT ANY WARRANTY; without even the implied warranty of
* _LINE_COMMENT_SYM     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* _LINE_COMMENT_SYM     GNU General Public License for more details.
* _LINE_COMMENT_SYM                           
* _LINE_COMMENT_SYM     Please use this URL to review the GNU General Public License:
* _LINE_COMMENT_SYM     http://www.gnu.org/licenses/gpl.txt             
* _LINE_COMMENT_SYM
* _LINE_COMMENT_SYM ============================================================= $trail_tag
* */
* 
*/
/*
*  * ---------------------------------------------------------------------------
*  * This source code is free software; you can redistribute it and/or modify
*  * it under the terms of the GNU General Public License as published by
*  * the Free Software Foundation; either version 2 of the License, or
*  * (at your option) any later version.
*  *
*  * This program is distributed in the hope that it will be useful,
*  * but WITHOUT ANY WARRANTY; without even the implied warranty of
*  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  * GNU General Public License for more details.
*  *
*  * Please use this URL to review the GNU General Public License:
*  *   http://www.gnu.org/licenses/gpl.txt
*  * ---------------------------------------------------------------------------
*/
int AFIchain( int file, char *fname, char *opts, char *path );
int AFIclose( int file );
int AFIgettoken( AFIHANDLE file, char *buf );
char *AFIisvar( char *tok, struct tokenmgt_blk *tmb );
char *AFIisvar( char *tok, struct tokenmgt_blk *tmb );
void *AFInew( int type );
int AFIopen( char *name, char *opts );
int AFIopenp( char *name, char *mode, char *path );
int AFIpushtoken( AFIHANDLE file, char *buf );
int AFIread( int file, char *rec );
int AFIreadp( int file, char *buf );
int AFIseek( int file, long offset, int loc );
void AFIsetflag( int file, int flag, int opt );
int AFIsetsize( int file, int num );
int AFIsettoken( AFIHANDLE file, Sym_tab *st, char *tsep, char varsym, char escsym, char *psep );
long AFIstat( int file, int opt, void **data );
int AFIwrite( int file, char *rec );
