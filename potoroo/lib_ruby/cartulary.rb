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



# ---------------------------------------------------------------------------------
# Mnemonic:	cartulary.rb
# Abstract:	provides an interface to the cartulary
#		NG_ROOT must be available via ppget if the cartulary 
#		path is not supplied to constructor
#		
# Date:		11 Jan 2008
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------

class Cartulary
	# -------------- class methods --------------------
	def self.get( key )
		if( $_ng_cartulary == nil )
			$_ng_cartulary = Cartulary.new( );		# create a default single object
		end

		return $_ng_cartulary.get( key );
	end


	# -------------- public instance methods ----------
	# take a string and attempt to replace $name references in the string with values from the cartulary.
	# being consistant with other ningaui standards, we support the expansion of just $vname and ${vname}
	# reference syntax. 
	def expand!( s )
		while( (ss = s.match( /\$[A-Za-z][A-Za-z0-9_]*/ ) ) != nil || (ss = s.match( /\$\{[A-Za-z][A-Za-z0-9_]*\}/ ) ) != nil )
			if( ss[0][1,1] == "{" )
				val = @cartulary.fetch( ss[0][2..-2], " " );
			else
				val = @cartulary.fetch( ss[0][1..-1], " " );
			end
			if( val != nil )
				s.gsub!( ss[0], val );
			end
		end
	end

	# read  the cartulary. expect lines of: [export] name=["]value["] #comment 
	# this method will be called automatically by get so that the values are refreshed every 5 minutes or so
	# the user app can invoke this if they think they know better about refreshing
	def refresh( )
		@cartulary = Hash.new( " " );

		@cartulary.store( "NG_ROOT", @ng_root );		# must be in from the start
		f = File.open( @path, "r" );

		f.each_line do |r| 
			r.lstrip!();			# strip leading whitespace 
			r.chomp!();
			r.gsub!( /=\"/, "=" );		# ditch the quotes, lead export and possible comment
			r.gsub!( /\".*/, "" );		
			r.gsub!( /^export /, "" );	# 
			r = r.rstrip( );			

			if( r.length() > 0 && r[0,1] != "#" )		# not a commented line
				a = r.split( "=" );		# split into "name" "value #comment";
				if( a.length() > 1 )
					expand!( a[1] );
					@cartulary.store( a[0], a[1] );
				end
			end
		end
		f.close( );

		@last_refresh = Time.now();
	end

	# look up a key in the hash and return the value. invoke refresh if we think 
	# it has been long enough since the last refresh.
	def get( key )
		if( @last_refresh + 300 < Time.now() )		# ensure cartulary is loaded every 5 minutes
			self.refresh();
		end
		
		return @cartulary.fetch( key, nil );
	end

	def initialize( path=nil )
		@path = "";
		@ng_root = "";

		@ng_root = ENV['NG_ROOT'];
		if( @ng_root == nil || @ng_root == "" )
			IO.popen( "ng_ppget NG_ROOT", "r" ) { |f| @ng_root = f.gets().chomp!(); }
		end

		if( path == nil )
			@path = @ng_root + "/cartulary";
		else
			@path = path;
		end

		self.refresh( );
	end
end


=begin
#---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(cartulary.rb:Ningaui Cartulary Interface)

&desc
	The cartulary is a file, usually &lit(NG_ROOT/cartulary) which contains Korn shell variable
	assignment statements of the form:
&space
&litblkb
   [export] variablename="value"  [# comment string]
&litblke
&space
	The cartulary is generally sourced (dotted in) by Kshell scripts in order to setup a 
	known and consistant environment.  This class provides a Ruby application with access to the 
	information in the current cartulary.

&space
&cmeth
&subcat get( vname ) => String, nil
	This class method allows the user application the ability to get the value for a variable 
	without creating an instance of the class. This method creates a global instance, if one 
	does not exist, and uses it.  Using the class method, and thus one instance of the cartulary
	object per application, is the most efficent interface to the cartulary.  Like the instance 
	&lit(get) method described later, the contents of the cartulary are refreshed approximately
	every five minutes in order to ensure that the most recently updated values are available to 
	the application without requiring a read of the cartulary file for each call.
&space
	This method searches the cartulary for the given variable name and returns the string value
	associated with the variable name or nil.
&space
&imeth	
&subcat	new( path=nil ) => Cartulary
	Creates and initialises the object using the information in &ital(path.) If &ital(path) is not given, then
	&lit(NG_ROOT/cartulary) is assumed.  The typical user appilcation will not need to create their own
	instance of this object; the use of the class method should be all that is needed. 
	The user application will need to create a new instance if a cartulary file other than the 
	standard, &lit(NG_ROOT/cartulary,) is to be used. 

&subcat get( vname ) => String, nil
	Returns the value associated with vname in the cartulary. If vname is not defined, then nil 
	is returned.  The cartulary will be reparsed and the object refreshed automatically when 
	this method is called if the last refresh occurred more than 5 minutes ago. 

&subcat refresh()
	Allows the user to refresh the object sooner than the automatic task. 

&space
&see
	&seelink(lib:ng_env) &seelink(lib:ng_nar_if) &seelink(pinkpages:ng_ppget) 

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	10 Jan 2008 (sd) : Its beginning of time. 
&term	25 Jan 2008 (sd) : Updated man page. 
&term	26 May 2009 (sd) : Now fetches NG_ROOT from environment hash if there rather than popen() call to ng_ppget.
		Falls back on ppget if not in the environment.
&endterms


&scd_end
=end
