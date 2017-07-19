#
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



# Mnemonic:	Argbegin
# Abstract:	Plan9 style command line argument processing with support
#		for --man manual page generation from self contained 
#		documentation. 
#
#		Features:
#			* automatically generates a usage string
#			* single definition of options list
#			* supports -p xxxx, -pxxx, and --port xxxx 
#		
#	
# 	For ningaui --man support, we assume several things:
#		NG_ROOT is defined
#		$NG_ROOT/lib contains the tfm startup files
#		tfm is executable via the current PATH
#		
# Date:		8 May 2008
# Author:	E. Scott Daniels
#
# --------------------------------------------------------------------------

module Argbegin

	MANDATORY = 1;
	OPTIONAL = 2;

	ELE_REQ = 0;		# element array field
	ELE_DATA = 1;
	ELE_CODE = 2;
	ELE_STR = 3;

				# description array fields
	DESC_REQ = 0;		# mandatory/optoinal flag (if synonyms given)
	DESC_COUNT = 0;		# count/range of expected positional parms (synonym list is nil)
	DESC_NAME = 1;		# string for usage

				# cud order:
	SYN_LIST = 0;		# array of synonyms
	DESC_LIST = 1;		# array of description information 
	CODE_BLOCK = 2;		# code to exec when parm is encounterd

	# --------------- class methods -------------------------------------
	# self contained documentation stuff
	# read the main programme file and generate self contained document 
	def self.format_man( fm_type="tfm", dest="stdout" )

		pname, junk = caller()[-1].split( ":", 2);	# get file name of parent programme

									# command components:
		setup = "XFM_IMBED=$NG_ROOT/lib; export XFM_IMBED;"; 	# env stuff that t/hfm needs
		init_cmd = ".im $XFM_IMBED/scd_start.im";		# initial t/hfm command to execute
		pager = "2>/dev/null ";					# silent errors
		if( dest == "stdout" )
			pager += "| ${PAGER:-more} ";			#  add pipe to more if writing to stdout
		end

		# build cmd inserting formatter type, input pname and output dest file
		cmd = sprintf( "%s %s %s %s %s %s", setup, fm_type, pname, dest, init_cmd, pager );
		system( cmd );
	end

	# generate a usage method 
	def self.usage( ustring=nil )
		pname, junk = caller()[-1].split( ":", 2);	# get file name of parent programme
		$stderr.printf( "usage: %s %s\n", pname, ustring ? ustring : "" );
	end


	# crack: parse command line arguments using the arrays of data passed in (cud)
	# each cud array contains 2 or 3 things:
	#
	#  format of each cud array:
	#  	[[synonym list],      [description list],   code-block]
	#	Where:
	#		synonym list is a list of option flags (e.g. "h", "help") that are to be recognised
	#		description list os the list of things that define the requirements of the option:
	#			[0]) mandatory or optional indicator
	#			[1]) name of data for usage; also serves as indication that a data element is needed
	#				(if no data is associated with the option, code nil)
	#		code-block is the block of code that is executed when the option is encountered. The code
	#			is invoked with two parameters: option_flag (e.g. -h) and data string. If data is 
	#			not required for the option, the data string will be nil. 
	#
	#	Positional parameters are checked against a special cud entry where first list is nil. The description 
	#		is assumed to be count|range and the usage string component.  If count is supplied, 
	#		an error is generated if the number of positional parameters does not exactly equal the 
	#		count.  Range values are interpreted as min..max such that:
	#			1..4	- requires at least 1 parm, and errors if there are more than 4
	#			0..4	- errors if there are more than 4, but allows none
	#			3..-1	- errors if thre are less than 3, and disables upper limit check 
	#
	#  Results:
	#	Positional parameters are combined into an array and returned to the caller. The contents of 
	# 	$* are NOT changed. 
	#	The global variable $argv0 is set to the name of the programme that is running (C argv[0])
	#------------------------------------------------------------------------------------------------------
	def self.crack( *cud )
		elements = Hash.new( nil );
		positional = nil;			# list of positional parameters to return
		ustring = "";
		parm_max = -1;
		parm_min = 0;			# default to exactly no parameters required

		if( $argv0 == nil )
			$argv0, junk = caller()[-1].split( ":", 2);	# get file name of parent programme
		end

		ustring = "";	

		cud.each() do |a|			# parse the cud arrays and generate the elements used to parse the commandline
			dl = a[DESC_LIST];
			mandatory = true;

			if( (sl = a[SYN_LIST]) == nil )				# defines positional parameter requirements
				ustring  += " " + dl[DESC_NAME];
				if( dl[DESC_COUNT].kind_of?( Range ) )		# range:  0..4  max of 4 parms  2..4 at least 2, max of 4  4..-1 at least 4
					parm_min = dl[DESC_COUNT].first;
					parm_max = dl[DESC_COUNT].last;
				else
					parm_min = parm_max = dl[DESC_COUNT];	# exactly this number required
				end
			else
				if( (dl = a[DESC_LIST]) == nil || dl[DESC_REQ] == OPTIONAL )
					mandatory = false;
					sep_sym = " [";
					close_sym = "]";
				else
					sep_sym = " ";
					close_sym = "";
				end

				datan = dl == nil ? nil : dl[DESC_NAME];	# data name; presence indicated data expected and gives string for usage
				
				edata = [mandatory, datan != nil, a[CODE_BLOCK], sl[0]]; 	# opt is mandotyry, opt takes data, code block
				sl.each() do |syn|					# build usage and add to element hash
					dashes = syn.length() > 1 ? "--" : "-";
					ustring += sep_sym + dashes + syn;
					sep_sym = "|";

					elements[dashes+syn] = edata;
				end

				if( datan != nil )
					ustring += " " + datan + close_sym;
				else
					ustring += close_sym;
				end
			end
		end

		if( elements["-?"] == nil )
			elements["--man"] = [false, false, lambda { self.format_man(); exit( 1 ); } ];
			elements["-?"] = [false, false, lambda { self.usage( ustring ); exit( 1 ); }];
		end
		
		i = 0;				# number of raw command line arg we are working with
		while( i < $*.length() ) 	# map the command line options to elements and drive user code
			p = $*[i];

			ele = nil;						# element that defines how to deal with option
			data = nil;
			if( positional == nil  &&  p == "--" )			# not yet to positionals and we hit -- 
				positional = Array.new();			# alloc array now to flip into positional collection state
			else
				if( positional != nil )					# now dealing with positional parms; just add to list
					positional << $*[i];
				else
					if(  p[0,1] == "-" )					# still processing options, and it is one
						if( (ele = elements[p]) != nil )		# an exact match of something like -p or --port
							if( ele[ELE_DATA] )			# parameter takes next as data
								i += 1;				# skip to get data
								data = $*[i];
							end
						else
							if( (ele = elements[p[0,2]]) != nil )	# something like -p1234 instead of -p 1234
								data = p[2..-1];
							else
								$stderr.printf( "unrecognised option: %s\n", p );
								ele = elements["-?"];		# stop short
							end
						end

						ele[CODE_BLOCK].call( p,  data );
						ele[DESC_REQ] = false;		# cheating, but this marks it as having been used
					else					# it is not an option; flip to positional processing
						positional = Array.new( );
						positional << $*[i];
					end
				end
			end
			

			i += 1;
		end

		mstring = "";
		sep_str = "";
		elements.each_value() do |ele|
			if( ele[ELE_REQ] )		# if never marked false, then we didn't encountere it
				$stderr.printf( "required option not found: %s\n", ele[ELE_STR] );
				mstring = "missing option";
				sep_str = " and ";
			end
		end

		found = positional == nil ? 0 : positional.length();
		if( parm_min > parm_max )		# must have at least min, but no max is enforced
			if( found < parm_min )
				mstring += sep_str + "minimum number of postional parameters (#{parm_min}) not found";
			end
		else
			if( found >= parm_min )
				if( found > parm_max )
					mstring = sep_str + "more than maximum expected postional parameters (#{parm_max}) found";
				end
			else
				mstring += sep_str + "minimum number of postional parameters (#{parm_min}) not found";
			end
		end

		if( mstring.length() > 0 )
			$stderr.printf( "%s\n", mstring   );
			self.usage( ustring );
			exit( 1 );
		end

		return positional;		# return list of positional parameters
	end
end


=begin
#------------------------------------------------------------------ 
# ningaui style documentation
&scd_start
&doc_title(Argbegin:Plan-9 like agurment sussing)
 
&space

&space
&desc	
	&This is a module containing a static method which is given a set of 
	option and positional parameter discriptors allowing it to parse the 
	command line elements (^$^*^[^]).  
	For each option that is encountered (e.g. -p) a user 
	supplied code block is executed to process the option and associated
	data.  
	Unlike other command line parsers, &this does not require a two 
	stage approach making the maintenance of the user programme much easier
	and less prone to errors during modification. 
	
&space
	The following is a list of features:
&space
&beglist
&item	Short (e.g. -h) and long (e.g. --help) options are supported.  Option synonmyms 
	are not limited in number so &lit(--info) could be added to the previously 
	mentioned options all having the same effect. 

&item	The &lit(--man) and &lit(-?) options are defined automatically, but can 
	be overridden by the user. 

&item	Options may be declared mandatory causing an error message to be generated
	and the programme to halt if one or more mandatory option is not discovered on 
	the command line. 

&item	An error message is generated, and the programme is halted, if an unrecognised 
	option is encountered.

&item	Old style option data pairs (-pVALUE) are supported without any extra programmer definition. 

&item	&this automatically builds the usage string.

&term	The count of expected positional parameters can be given as a hard value, as a range:
&space
&begterms
&term	n : exactly n parameters must be present.
&term	n..m : at least n, but not more than m; n may be 0. 
&term	n..-1 : at least n, and no upper bound; n may be 0. 
&endterms
&endlist
&space

&space
	To make use of the &lit(--man) option, &this assumes that the NG_ROOT environment variable is defined and 
 	references a legitimate directory with ningaui installed. It also assumes
 	that the &lit(tfm) binary is in a directrory contained in the current &lit(PATH.)

&cmeth	
&subcat Argbegin::crack( syn-list, desc-list, code... )
	This method accepts one or more specification arrays which describe each option set to be 
	recognised.  Each array consists of a list (array) of synonmyms, a list (array)
	of descrition information, and a code block.  
&space
&subcat	The synonym list
	The list of synonyms is an array of strings (.e.g "h" for the option "-h") that 
	are recognised and when encountered cause &this to drive the supplied block of code. 
	The lead dash character(s) (-) are automatically assumed, and if the same block of 
	code can be used to support multiple options, they all may be listed in the same 
	array. 
&space
&subcat	The description list
	The description provides the indication of whether or not the option is mandatory, 
	and the indication that the option is followed by a data token.  The 
	mandatory/optional indicator is supplied using one of two class constants:
&space
&litblkb
   Argbegin::OPTIONAL, Argbegin::MANDATORY
&litblke
&space
	The data token indicator is a string which both signals that a data token follows
	the option on the command line, but gives the string to be used in the usage message.
	If the option does not require a data token, &lit(nil) should be supplied. If the 
	option is not required, and does not need a data token, then the list itself 
	may be &lit(nil) in the specification.

&space
&subcat	The code block
	The final element of a specification array is a block of code that &this will execute
	when the option is encountered.  It is created using either &lit(lambda) (preferred) 
	or &lit(proc) commands, and should expect to be passed to parameters: The option 
	string and the data string.  The following example takes the data value, converts
	it to integer and saves it:
&space
&litblkb
   lambda { |p, str| port = str.to_i(); }
&litblke
&space
	

&space
&subcat Positional Parameters
	The only error checking/processing that &this performs on the positional 
	parameters is to verify a count, and to return them in an array to the 
	caller. 
	Positional parameter counts, and the usage message descriptive text, are 
	supplied using a specification array which has a &lit(nil) synonym list. 
	The description list contains a count, or range, and the string that is 
	used at the end of the usage message to describe the positional parameters.
	The count is either a hard number of postional parameters, or a range as described
	earlier which indiate a number of required and variable positional parameters
	in a min/max format. 

&space
&subcat	Return Value
	Once the command line contents have been processed, &this returns an array 
	of the remaining positional parameters, or &lit(nil) if no parameters 
	were entered.  
&space
&subcat Example
	The specification arrays may be placed in any order; the only affect is the 
	order that the usage message is constructed.  
	The folowing is a small sample of how to invoke this method to parse
	the command line which requires at least three command line parameters:

&litblkb
require "argbegin";

interactive = false;		# must predeclare to scope outside
nnsd_port = 0;
port = 0;
verbose = 0;
plist = Argbegin.crack(  
   [["d", "nnsd"],     [Argbegin::OPTIONAL, "nnsd-port"],  lambda { |p, str| nnsd_port = str.to_i(); }],
   [["p", "port"],      [Argbegin::MANDATORY, "n"],     lambda { |p, str| port = str.to_i(); } ],
   [["v"],              [Argbegin::OPTIONAL, nil],      lambda { |p, str| verbose += str != nil ? str.to_i() : 1; }],
   [["i", "interact"], 	nil,                            lambda { interactive = true; }]
   [nil,		[3..-1,"filename filename host [host...]" ]]
);
&litblke
&space
	The definition of the "v" option and the "i" option are the same as far as
	the data requirements are concerned.  The defaults are OPTIONAL and no 
	data and thus passing &lit(nil) as the description accomplishes this. 

&subcat	Argbegin::format_man( type="tfm", dest="stdout" )
	This mehod may never be needed externally, but can be invoked by any programme.

	This method uses the name of the 'main' programme as the input file for the 
	text formatter.  The formatter is forced to skip to the opening scd tag
	&lit(^&scd_start) and will process until the end of the file or until the 
	end scd tag &lit(^&end_scd) is encountered.  
	Formatted text is written to the standard output device and piped to the
	pager programme specified by the &lit(PAGER) environment variable or &lit(more)
	if the environment varialbe is not defined. 
 
&space
&mods
&owner(Scott Daniels)
&lgterms
&term	08 May 2007 (sd) : Its beginning of time. 
&term	29 Jun 2009 (sd) : Now does the detection of range object correctly. 
&term	12 Aug 2009 (sd) : Corrected typo in the manpage. 
&endterms

&scd_end
#------------------------------------------------------------------ 
=end

