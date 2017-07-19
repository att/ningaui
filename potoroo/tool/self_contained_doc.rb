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


# DEPRECATED -- lib_ruby/argbegin.rb replaces this!


# self contained documentation class
# class supports ningaui style self contained documentation
# 
# we assume several things:
#	NG_ROOT is defined
#	$NG_ROOT/lib contains the tfm startup files
#	tfm is executable via the current PATH
#
# To use this, assign the man page XFM source to a a single string
# (here doc format works best) and then invoke the Self_contained_doc 
# show_man() class method passing it the string. 
#
# --------------------------------------------------------------------------
require "digest/md5"

class Self_contained_doc

	# format mp (string) using fmtype (tfm/hfm) and write to dest
	def self.show_man( mp, fm_type="tfm", dest="stdout" )
		if( mp == nil )
			$stderr.printf( "show_man: missing/empty manual page string" );
			return;
		end


		# we put scd source from string to a file for t/hfm to read; generate random filename
		fname = "/tmp/scd." + Digest::MD5.new.update( "%x" % (Time.now.to_i + rand( mp.size() )) ).hexdigest;

									# command components:
		setup = "XFM_IMBED=$NG_ROOT/lib; export XFM_IMBED;"; 	# env stuff that t/hfm needs
		init_cmd = ".im $XFM_IMBED/scd_start.im";		# initial t/hfm command to execute
		pager = "2>/dev/null | ${PAGER:-more} ";		# redirection of error and pipe to pager

		# build cmd inserting formatter type, input fname and output dest file
		cmd = sprintf( "%s %s %s %s %s %s", setup, fm_type, fname, dest, init_cmd, pager );

		f = File.new( fname, "w" );
		f.printf( "%s\n", mp );
		f.close( );

		system( cmd );
		system( "rm -f " + fname );
	end
end


=begin
#------------------------------------------------------------------ 
&scd_start
&doc_title(Self_contained_doc:Generate manpage from ruby script)
 
&space

&space
&desc	
	NOTE: This module is deprecated; use lib_ruby/argbegin.rb instead.

&space
	&This class provides a class method that allows the calling programme to 
 	have the XFM document source for its manual page formatted and displayed
 	on the standard output device. 
&space
 	This class assumes that the NG_ROOT environment variable is defined and 
 	references a legitimate directory with ningaui installed. It also assumes
 	that the &lit(tfm) binary is in a directrory contained in the current &lit(PATH.)

&cmeth	
&subcat	Self_contained_doc::show_man( string );
	Causes the contents of &ital(string) to be passed to the proper XFM formatter
	and displayed on the standard output device. The currently defined pager
	(environment variable PAGER) is used; &lit(more) is used if &lit(PAGER) is 
	not defined. 
 
 
&space
&mods
&owner(Scott Daniels)
&lgterms
&term	07 Nov 2007 (sd) : Its beginning of time. 
&term	14 Jan 2008 (sd) : Updated the documentation. 
&endterms

&scd_end
#------------------------------------------------------------------ 
=end

