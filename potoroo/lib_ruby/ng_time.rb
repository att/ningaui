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
# Mnemonic:	ng_time
# Abstract:	time interface that can mimic/translate ng_time.c timestamp values
#		ningaui timestamps are based on a 1994 epoch, are in tenths of 
#		seconds, and based on unadjusted (dst) local time.
#		
# Date:		15 Jan 2008
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------

class Ng_time
	# ----------------- class methods ----------------------------------------
	INTEGER = 1;			# format constants
	COMPRESSED = 2;
	PRETTY = 3;
	LONG_PRETTY = 4;
	TIMESTAMP = 5;

	NG_OFFSET = 757400400;			# difference between ningaui and unix epochs
	#NG_OFFSET = 757382400;

	# convert the unix time in the uts object to generate a ningaui time in some form
	def self.cvtuts( uts, format=INTEGER )
		case( format )
			when LONG_PRETTY; 	return uts.to_s;
			when PRETTY;		return uts.strftime( "%Y/%m/%d %H:%M:%S" );
			when COMPRESSED;	return uts.strftime( "%Y%m%d%H%M%S" );
			when TIMESTAMP;		
						zchr = uts.strftime( "%Z" )[0,1];
						return self.cvtuts( uts, INTEGER).to_s  + "[#{uts.strftime( "%Y%m%d%H%M%S" )}#{zchr}]";
			else INTEGER;		return (uts.to_i - NG_OFFSET)*10;	
		end
	end

	# get the current time and return a ningaui integer time stamp (default) or a log style timestamp string
	def self.now( format=INTEGER )
		n = Time.now();
		if( n.dst?() )
			n += 3600;
		end
		return self.cvtuts( n, format );
	end

	# convert ningaui timestamp to the indicated format
	def self.cvt( nts, format=COMPRESSED )
		uts = (nts/10) + NG_OFFSET;
		if( format == INTEGER )
			return uts;				# just the integer we computed
		end

		ut = Time.at( uts );
		if( ut.dst?() )
			ut -= 3600;
		end
		return self.cvtuts( ut, format );	# others require exta efort
	end


	public
	# ----------------------------- constructor and instance methods ---------------------
	# build an obj from a yyyymmddhhmmss string
	def initialize( tstring=nil )
		@utime = nil;

		if( tstring == nil )
			@utime = Time.now();
		else
			case( tstring.length() )
				when 4;		tstring += "0101000000";
				when 6;		tstring += "01000000";
				when 8;		tstring += "000000";
				when 10;	tstring += "0000";
				when 12;	tstring += "00";
				when 14;

				else
					tstring = nil;
					@utime = Time.now();
			end	
		end

		if( @utime == nil && tstring != nil )
			@year = tstring[0,4];
			@mon = tstring[4,2];
			@day = tstring[6,2];
			@hr = tstring[8,2];
			@min = tstring[10,2];
			@sec = tstring[12,2];

			
			@utime = Time.local( @year, @mon, @day, @hr, @min, @sec );
		end

	end

	def to_s( format=PRETTY )
			return Ng_time::cvtuts( @utime, format );
	end

	def to_pretty( long=false )
		if( long )
			return Ng_time::cvtuts( @utime, LONG_PRETTY );
		else
			return Ng_time::cvtuts( @utime, PRETTY );
		end
	end

	def to_ui()			# return unix integer time
		return @utime.to_i;
	end

	def to_i()
		if( @utime.dst?() )
			return Ng_time::cvtuts( @utime+3600, INTEGER );
			
		else
			return Ng_time::cvtuts( @utime, INTEGER );
		end
	end

	def to_ts()
		return Ng_time::cvtuts( @utime, TIMESTAMP );
	end

	def to_compressed()
		return Ng_time::cvtuts( @utime, COMPRESSED );
	end
end




=begin
#---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_time.rb:Ningaui Time Interface)

&desc 
	The &this class provides the means to obtain Ningaui timestamps, which 
	are defined to be tenths of seconds with an epoch of January 1, 1994, and
	to convert between UNIX time and ningaui time. 
&space
	This class provides only class methods; an instance of this class cannot
	be created.
	
.** class constants
&cconst
	These constants are supported:
&begterms
&term	PRETTY : Convert timestamp, to a more human readable form: YYYY/MM/DD HH:MM:SS. 
&space
&term	LONG_PRETTY : Convert timestamp, to a longer, standardised, format like: Tue Jan 15 15:29:24 -0500 2008 
&space
&term	COMPRESSED : Convert timestamp to a compressed format: YYYYMMDDHHMMSS
&space
&term	TIMESTAMP : Generate a ningaui masterlog timestamp which has a leading 
	integer component with a compressed timestamp in square braces concatenated to it
	(e.g. 4201234360[20060422190000]).
&space
&term	INTEGER : Generate an integer timestamp. 
&endterms

.** class methods
&space
&cmeth
&subcat now( format=Ng_time::INTEGER )
	Get the current time and convert to the indicated format. 
&space
&subcat cvt( timestamp, format=Ng_time::COMPRESSED )
	Converts the ningaui integer &ital(timestamp) to the desired format.  
	format is set to Ng_time::INTEGER, then it is converted to the 
	equivalant UNIX timestamp. 
	
&space
&subcat cvtuts( unix_timestamp, format=Ng_time::INTEGER )
	This method converts the supplied unix integer timestamp into the desired 
	format. If &ital(formmat) is omitted, the corresponding ningaui integer 
	timestamp is returned. 

.** constructor(s)
&space
&construct
&subcat	new( compressed_time=nil  ) --> Ng_time
	Creates a ningaui time object for the indicated time (now if no argument supplied).
	Compressed time is of the form YYYYMMDDHHMMSS where all but the year is optional 
	(e.g. 200904 results in 2009/04/01 00:00:00).

.** instance methods
&space
&imeth	
&subcat	to_i --> int
	Returns the time into a ningaui integer timestamp.

&subcat to_ui --> int
	Converts the time into a Unix integer (seconds since the epoc). 

&subcat to_compressed --> string
	Returns the time converted into a compressed (YYYYMMDDHHMMSS) string. 

&subcat to_pretty(long-false) --> string
	Returns a string containing the time formatted into a pretty string. 
	If true is passed in, a long-pretty string is generated.

&subcat to_ts --> string
	Returns a string containing the time formatted into a ningaui master log style 
	timestamp.
	
&space
&examp
	Get the current time as an integer timestamp:
&litblkb
  now = Ng_time.now();
&litblke
&space
	
	Get the current time as a pretty string:
&litblkb
  now = Ng_time.now( Ng_time::PRETTY );
&litblke

	Convert a ningaui timestamp into a pretty string:
&litblkb
  s = Ng_time.cvt( now, Ng_time::PRETTY );
&litblke
&space

	Convert a ningaui timestamp into an integer based on the UNIX epoch:
&litblkb
  uits = Ng_time.cvt( now, Ng_time::INTEGER );
&litblke
&space



&space
	

&space
&see	&seelink(lib:ng_time) &seelink(tool:ng_dt)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	15 Jan 2008 (sd) : Its beginning of time. 
&term	25 Jan 2008 (sd) : Updated man page. 
&term	12 Jun 2009 (sd) : Added ability to instance the class. 
&endterms


&scd_end
=end

