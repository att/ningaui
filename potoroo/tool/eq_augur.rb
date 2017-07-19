#!/usr/bin/env ruby
# ======================================================================== v1.0/0a157 
#                                                         
#       This software is part of the AT&T Ningaui distribution 
#	
#       Copyright (c) 2001-2009 by AT&T Intellectual Property. All rights reserved
#	AT&T and the AT&T logo are trademarks of AT&T Intellectual Property.
#	
#       Ningaui software is licensed under the Common Public
#       License, Version 1.0  by AT&T Intellectual Property.
#                                                                      
#       A copy of the License is available at    
#       http://www.opensource.org/licenses/cpl1.0.txt             
#                                                                      
#       Information and Software Systems Research               
#       AT&T Labs 
#       Florham Park, NJ                            
#	
#	Send questions, comments via email to ningaui_support@research.att.com.
#	
#                                                                      
# ======================================================================== 0a229


#!/usr/bin/env ruby
# Mnemonic:	eq_augur (an ancient roman offical charged with interpreting
#			oomens and providing guidence in public affaris)
# Abstract:	Read an eqreq 'gen' table and descirbe the differences between it
#		and the current world; indicate what must be done to get to the 
#		'gen' state. 
#		
# Date:		30 April 2008 (22 and growing)
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------


# gen-data information columns
FID_COL = 0
LAB_COL = 1;			# label name
MNT_COL = 2;			# mount point directory
TYPE_COL = 3;			# device type
OPT_COL = 4;			# options
SRC_COL = 5;			# source (comment token)
FLG_COL = 6;
HLIST_COL = 7;			# host list (comma separated host:rw)

DEFAULT = 0;			# actions 
GEN_TABLE = 1;			# -g -- generate a gen table rather than computing changes 

# ------------------ internal classes --------------------------------------------

# returns true if the tokens in l1 (string of space sep tokens) is same as in l2 (order not important)
# or if both are nil.
def same_tokens?( l1, l2, sep=" " )
	if( l1 == nil )
		if( l2 == nil )
			return true;
		end
	else
		if( l2 == nil )
			return false;
		end
	end

	l1tok = l1.split( sep );
	l2tok = l2.split( sep );

	count = Hash.new( 0 );
	l1tok.each() { |t| count[t] = count[t] + 1; }
	l2tok.each() { |t| count[t] = count[t] + 1; }

	state = true;
	count.each_value() do |c| 
		if( c < 2 )
			state = false;
		end
	end

	return state;
end

class Fs_target			# a file system for equerry to manage
	def initialize( name )
		@name = name;
		@chost = nil;		# current host for rw mount
		@fhost = nil;		# future host

		@chlist = "drop";		# current things; host list
		@ctype = ".";
		@cmtpt = ".";
		@cmount = ".";
		@csrc = "unk";
		@clabel = ".";
		@copt = ".";
		@cflags = ".";

		@fhlist = "";		# future things (from gen data)
		@ftype = "ufs";
		@fmtpt = ".";
		@fmount = ".";
		@fsrc = "unk";
		@flabel = ".";
		@fopt = ".";
		@ffsid = ".";
		@fflags = ".";
	end

	# print a 'gen' table entry for the fs target
	def print_gen()
		if( @clabel != nil )
			$stdout.printf( "%-14s %-30s %-15s %-15s %-15s %-15s %-15s %-15s\n",  @cfsid, @clabel, @cmount, @ctype, @copt, @csrc, @cflags, @chlist );
		else
			$stdout.printf( "%-14s %-30s %-8s %-8s %-8s %-8s %-8s %-8s\n",  @ffsid, @flabel, @fmount, @ftype, @fopt, @fsrc, @fflags, @fhlist );
		end
	end

	def print_old_new( tag, old, new )
		$stdout.printf( "\n\t" ) if( $long_format );

		if( old == nil || old == "" || old == new  || $verbose < 2 )
			$stdout.printf( "   %s(%s)", new, tag );
			#$stdout.printf( " %5s: %s", tag, new );
		else
			#$stdout.printf( " %5s: %s -> %s", tag, old, new );
			$stdout.printf( "   %s -> %s(%s)", old, new, tag );
		end
	end

	# print parm info
	def print_parms( all=false )
		if( @clabel != @flabel || all )
			print_old_new( "label", @clabel, @flabel );
		end
		if( @ctype != @ftype || all )
			print_old_new( "type", @ctype, @ftype );
		end
		if( @cmtpt != @fmtpt || all )
			print_old_new( "mtpt", @cmtpt, @fmtpt );
		end
		if( @cmount != @fmount || all )
			print_old_new( "mount", @cmount, @fmount );
		end
		if( @csrc != @fsrc || all )
			print_old_new( "src", @csrc, @fsrc );
		end
		if( @copt != @fopt || all )
			print_old_new( "opt", @copt, @fopt );
		end
		if( all || ! same_tokens?( @cflags, @fflags, "," ) )
			print_old_new( "flags", @cflags, @fflags );
		end
		if( @chlist != @fhlist || all )
			print_old_new( "hlist", @chlist, @fhlist );
		end
	end

	def print( )
		if( @fhost == nil )
			$stdout.printf( "DROP: %14s %10s\n", @name, @chost );
		else
			if( @chost == nil )
				$stdout.printf( "NEW:  %14s %21s  ", @name, @fhost );
				print_parms( true );
			else
				if( @chost != @fhost )
					$stdout.printf( "MOVE: %14s %10s %10s  ", @name, @chost, @fhost );
				else
					if( self.changing_parms?() )
						$stdout.printf( "PARM: %14s %10s %10s  ", @name, @chost, @fhost );
					else
						if( $verbose > 0 )
							$stdout.printf( "SAME: %14s %10s %10s  ", @name, @chost, @fhost );
						end
					end
				end

				print_parms( $verbose > 0 );

			end
			$stdout.printf( "\n" );
		end
		$stdout.printf( "\n" ) 	if( $long_format );
	end

	def changing_host?()				# true if this fs is changing hosts
		return true if( @chost != @fhost );
	end

	def changing_parms?()				# true if this fs has parms that change
		return true if( @ctype != @ftype );
		return true if( @csrc != @fsrc );
		return true if( @clabel != @flabel );
		return true if( @cmtpt != @fmtpt );
		return true if( @cmount != @fmount );
		return true if( @chlist != @fhlist );
		return true if( @cflags != @fflags );
		#return true if( @cfsid != @ffsid );
		return false;
	end

	def changing?()						# true if anything changing
		return true if( @chost != @fhost );
		return changing_parms?();
	end

	def istargetof?( thost )		# true if the future host is thost
		return @fhost == thost;
	end

	def stale?()				# true if fs is being removed
		return fhost == nil;
	end

	def fresh?()				# true if fs is new
		return chost == nil;
	end

	def set_chost( h ) @chost = h; end		# tedious, but accessors are sloppy if you ask me.
	def get_chost( ) return @chost; end

	def set_fhost( h ) @fhost = h; end
	def get_fhost( ) return @fhost; end

	def set_ctype( t ) @ctype = t; end
	def get_ctype( ) return @ctype; end

	def set_csrc( t ) @csrc = t; end
	def get_csrc( ) return @csrc; end

	def set_chlist( h ) @chlist = h; end
	def get_chlist( ) return @chlist; end

	def set_cmtpt( m ) @cmtpt = m; end
	def get_cmtpt( ) return @cmtpt; end

	def set_cmount( m ) @cmount = m; end
	def get_cmount( ) return @cmount; end

	def set_copts( m ) @copt = m; end
	def get_copts( ) return @copt; end

	def set_clabel( m ) @clabel = m; end
	def get_clabel( ) return @clabel; end

	def set_cfsid( m ) @cfsid = m; end
	def get_cfsid( ) return @cfsid; end

	def set_cflags( m ) @cflags = m; end
	def get_cflags( ) return @cflags; end

	def set_ftype( t ) @ftype = t; end
	def get_ftype( ) return @ftype; end

	def set_fmtpt( m ) @fmtpt = m; end
	def get_fmtpt( ) return @fmtpt; end

	def set_fmount( m ) @fmount = m; end
	def get_fmount( ) return @fmount; end

	def set_fhlist( h ) @fhlist = h; end
	def get_fhlist( ) return @fhlist; end

	def set_fsrc( t ) @fsrc = t; end
	def get_fsrc( ) return @fsrc; end

	def set_flabel( m ) @flabel = m; end
	def get_flabel( ) return @flabel; end

	def set_fopts( m ) @fopt = m; end
	def get_fopts( ) return @fopt; end

	def set_fflags( m ) @fflags = m; end
	def get_fflags( ) return @fflags; end

	def set_ffsid( m ) @ffsid = m; end
	def get_ffsid( ) return @ffsid; end
end

# ----------------------- static methods ---------------------------------------------------



# build a pool of filesystems from input 'gen' data
# this creates the 'desired future' world view
def bld_pool( gf=$stdin )

	def_mtptd = ".";
	def_devd = "/dev/ufs";
	def_devd = "label_only"; 		# force unqualified entreies in device col to be treated as labels

	pool = Hash.new();

	# parse the gen-data input
	gf.each_line() do |buf|
		buf.chop!();					# ditch newline
	
		if( buf.length > 0 )				# skip blank lines
			tokens = buf.split( );			# split on space
			#fsname = tokens[LAB_COL];		# version 1 used last bit of lable as filesystem id
			#fsname.gsub!( /.*\//, "" );
			fsname = tokens[FID_COL];		

			case( buf[0, 1] )
				when "!"
					if( tokens[0] == "!default_mtpt_dir" )
						def_mtptd = tokens[1] == "=" ? tokens[2] : tokens[1];		# allow !default_mtpt_dir [=] path
					else
						if( tokens[0] == "!default_device_dir" )
							def_devd = tokens[1] == "=" ? tokens[2] : tokens[1];
						end
					end
		
				when "#"						# skip comments 
					;
		
				else
					hlist = tokens[HLIST_COL].split( /[:,]/ );
					thost = nil;
					ihost = nil;
					(0..(hlist.length()-1)).step( 2 ) do |i| 		# find target (rw) and ignore host
						if( hlist[i+1] == "rw" )
							thost = hlist[i];	
						else
							if( hlist[i+1] == "ignore" )		# we assume ignore host is target if no :rw host found
								ihost = hlist[i];
							end
						end
					end				
			
					if( hlist[0] != "drop" );
						fst = pool[fsname];
		
						if( fst == nil )
							fst = pool[fsname] = Fs_target.new( fsname );
						end
		
						fst.set_fhost( thost == nil ? ihost : thost );
		
						if( tokens[LAB_COL][0, 1] == "/" )
							fst.set_flabel( tokens[LAB_COL] );
						else
							if( def_devd != "label_only" )
								fst.set_flabel( def_devd + "/" + tokens[LAB_COL] );
							else
								fst.set_flabel( tokens[LAB_COL] );
							end
						end

						fst.set_ffsid( tokens[FID_COL] );
						fst.set_fflags( tokens[FLG_COL] );
						fst.set_fhlist( tokens[HLIST_COL] );
						fst.set_fopts( tokens[OPT_COL] );
						fst.set_ftype( tokens[TYPE_COL] );
						fst.set_fsrc( tokens[SRC_COL] );
						if( tokens[MNT_COL] == "." )
							fst.set_fmtpt( def_mtptd );
							fst.set_fmount( "#{def_mtptd}/#{fsname}" );
						else
							fst.set_fmount( tokens[MNT_COL] );
							i = tokens[MNT_COL].rindex( "/" );
							if( i > 0 )
								fst.set_fmtpt( tokens[MNT_COL][0..i-1] );
							else
								fst.set_fmtpt( def_mtptd );
							end
							
						end
					end
			end
		end
	end

	return pool;
end			# end build pool 


# update the targets in the pool with current settings listed by  narbalek
# from nar vars, decompose into parts:	 
#		equerry/rmfs0234:parms="ALLOC PRIVATE NOACTION label=/dev/ufs/rmfs0234 type=ufs src=pk22 mount=/l/rmfs0234 mtpt=/l"        #  1209644089
#		equerry/rmfs0001:host="ant01:rw"        #  1209154399
	
def read_current( pool, add2pool=false )
	sp = 0;
	ci = IO.popen( "ng_nar_get|gre \"equerry.*host|equerry.*parm\"" );	# build a list of existing fs mappings
	ci.each_line() do |buf|
		buf.chop!();
		name, value = buf.split( "=", 2 );		# split just on first = (others appear in parms) 
		
		junk, fsid, vtype = name.split( /[\/:]/ );			# split out fs id and the variable type (host/parm)
		value.slice!( /[ 	]+#.*/);				# ditch trailing comment and whitespace before it
		value.gsub!( "\"", "" );					# ditch quotes
	
		if( (fst = pool[fsid]) == nil && add2pool )				# add to pool if not there and allowed
			fst = pool[fsid] = Fs_target.new( fsid );
			fst.set_cfsid( fsid );
		end
	
		if( fst != nil )
			case( vtype )
			when "host"
				fst.set_chlist( value );					# keep the collection of hosts
				hlist = value.split( /[ \t:]/ );
	
				thost = nil;
				(0..(hlist.length()-1)).step( 2 ) do |i|
					if( hlist[i+1] == "rw" || (hlist[i+1] == "ignore" && thost == nil) )
						fst.set_chost( hlist[i] )			# current target host
					end
				end
	
			when "parms"
				tokens = value.split( /[ \t]/ );
				0.upto( tokens.length() - 1 ) do |i|
					pname, pval = tokens[i].split( /=/, 2 );		# some values contian = so be careful
					case( pname )
						when "NOACTION",  "PRIVATE"
								cf = fst.get_cflags();
								if( cf == "." )
									fst.set_cflags( pname );
								else
									fst.set_cflags( pname + "," + cf );
								end

						when "label"; 	fst.set_clabel( pval );
							
						when "src"; 	fst.set_csrc( pval );
	
						when "type"; 	fst.set_ctype( pval );
	
						when "mtpt"; 	fst.set_cmtpt( pval );

						when "mount";	fst.set_cmount( pval );

						when "opts";	fst.set_copts( pval );
					
						# ignore all other stuff
					end
				end
			end
		end
	end

	ci.close();
end				# end update pool


# ---------------------- main   -----------------------------------------------------------

$verbose = 0;			# -v turns on
$long_format = false;		# -l turns on
list_all = false;		# -a turns on 
forreal = true;
action = DEFAULT;

i = 0;

require( "argbegin" );
pparms = Argbegin.crack(
	[["a", "all"], 		nil, 	lambda { list_all = true; } ],
	[["g", "gentable"], 	nil, 	lambda { action = GEN_TABLE } ],
	[["l", "long"], 	nil, 	lambda { $long_format = true } ],
	[["n", "noexec"], 	nil, 	lambda { forreal = false } ],
	[["v"], 		nil,	lambda { $verbose += 1; } ],
	[nil, [0..1, "[<]filename"]]
);


	

case( action )
	when GEN_TABLE				# produce a gen table to stdout
		pool = Hash.new( nil );
		read_current( pool, true );		# read current and allow updates 
		pool.each_key() do |k|
			pool[k].print_gen( );
		end

	else						# default -- read desired world from gen file and list differences between current state and desired
		if( pparms != nil )
			gf = File.open( pparms[0] );
		else
			gf = $stdin;
		end
		
		pool = bld_pool( gf );		# read gen info to build a pool of fs targets
		gf.close();
		
		read_current( pool, true );			# update the pool with current information;
		
		
		
		if( ! $long_format )
			$stdout.printf( "CHNG  %14s %10s %10s  %s\n", "FSNAME", "OLD", "NEW/CUR", "   PARM INFO" );
		end

		pool.each_value() { |pe| pe.print() if( list_all || pe.changing?() ); }	   # show all pool elements that have changes
end

exit( 0 );


=begin
#---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_eq_augur:List chagnes between gen table and current state)


&space
&synop	ng_eq_augur [-a] [-g] [-l] [-v] [<]gen-data

&space
&desc	
	By default, &this will compare the &ital(gen-data) file passed in 
	with the current environment (equerry settings in narbalek) and list 
	the changes that would result if the &ital(gen_data) were parsed by 
	&lit(ng_eqreq) and the commands executed. 
	The output is of the form:
&space
&litblkb
   change-type  fsid  onode nnode parameter-info
&litblke
&space

	Where:
&begterms
&term	change-type : Indicates move, new, parm (one or more parameter changes), drop or same.
	Unchanged file system descriptions, same, are listed only of the all (-a) flag 
	is placed on the command line. 

&space
&term	fsid : Is the ID string assigned to the filesystem (also used as the unique portion of 
	an equerry variable name).
&space
&term	onode : Indicates the old node if the filesystem is being moved. For all other 
	change types this field is blank.
&space
&term	nnode : Indicates the new node in the case of a move, or the node to which the filesystem 
	is assigned for all other change types. 
&space
&term	parameter-info : Lists the &stress(new) values of parameters that have changed.  If a 
	single versbose option (-v) is presented on the command line, then all parameter settings
	are listed.  If more than one verbose option is presented on the command line, then 
	all parameters are listed and those that are changing are indicated in an &lit(old -> new)
	format. Parameters are listed using a value(name) syntax.  
&endterms
&space
	By default, &this assumes that the equerry gen table is to be parsed as though
	&lit(!default_device_dir) is set to &lit(label_only.) This causes all non-qualified 
	tokens in the device/label column of the gen table to be treated as lables and not 
	the last portion of a default device path.  If the &lit(!default_device_dir) directive
	is set in the gen, &this will recognise it and start to use it in the same manner as
	equerry would interpret it.  Using this default makes it easy for a table gen 
	from the current set of fstab files on a cluster to be parsed. 

&subcat	Generating A Table
	A "gen" table that can be read by the ng_eqreq gen command can be generated from the 
	current narbalek variable settings.  The &lit(-g) option enables this mode and causes
	the table to be written to the standard output device. 


&space
&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-a : List all filesystems, even the ones that have no changes. 
&space
&term	-g : Generate &lit(ng_eqreq) gen-data for the current settings in narbalek.
&space
&term	-l : Long list.  The data for each filesystem is split on multiple lines. 
&space
&term	-v : Verbose mode.  Mostly affects the listing of parameter data and the 
	way parameters are listed. 
&endterms


&space
&parms
	The gen-data filename may be supplied as the only positional parameter. If missing, 
	&this reads gen-data from the standard input device. 

&space
&exit
	Right now this should always exit with a zero return code. 

&space
&see
	&seelink(tool:ng_equerry) &seelink(tool:ng_eqreq)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	30 Apr 2008 (sd) : Its beginning of time (HBDKAD). 
&term	12 May 2008 (sd) : Converted to Argbegin option parsing. 
&term	03 Oct 2008 (sd) : Cleaned up and added flags that support noaction and private flags. 
&term	03 Apr 2009 (sd) : Fixed typo in man page. .** probabley others remain unnoticed. 
&term	16 Jul 2009 (sd) : Corrected a problem with putting out current options.
&endterms


&scd_end
=end
