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
# Mnemonic:	ng_log.rb
# Abstract:	provides an interface to ningaui log environment
#		
#		
# Date:		15 Jan 2008
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------
require "cartulary"
require "ng_time"
require "broadcast"

if( $verbose == nil )
	$verbose = 0;
end
if( $verbose_catigory == nil )
	$verbose_catigory = 0;
end


class Ng_log
	ALERT = 1;
	CRIT = 2;
	ERROR = 3;
	WARN = 4;
	NOTICE = 5;
	INFO = 6;

	# catigory flags -- more efficent to hard code (11) rather than Ng_log.bleat( VBC_LIBRARY + 1...)
	VBC_ALL	= 0x00			# bleat on any catigory
	VBC_LIBRARY = 0x01
	VBC_NETWORK = 0x02

	# ----------- class methods -----------------------------

	# we only need one instance of ng_log, so users should invoke the class method
	# which will create the instance and then funnel arguments to the real printf() 
	# method.
	def self.printf( priority, fmt, *args )
		if( $_ng_log == nil )
			if( $_argv0 == nil )
				c = caller(0);
				a = c[-1].split('.');
				if( a[0] != nil && a[0] != "" )
					$_argv0 = a[0];
				else
					$_argv0 = "undetermined";
				end
			end

			$_ng_log = Ng_log.new( $_argv0 );
		end

		$_ng_log.fmt_send( priority, fmt, args );
	end

	def self.bleat( level, fmt, *args )
		i = 0;

		vmask = $verbose/10;
		catigory = level / 10;
		if( catigory == 0 ||  (catigory & vmask != 0) )
			if( $verbose % 10 >= level % 10  )
				if( args == nil )
                        		ustr = fmt;
                		else
                        		ustr = fmt.gsub( /%[0-9]*[.]*[0-9]*[dflsx%]/ ) do |m|
                                		if( i < args.length() )
                                        		i += 1;                         # must incr first so this does not become the return value
                                        		sprintf( m, args[i-1] );
                                		end
                        		end
                		end
		
				
				ustr.gsub!( /\n/, "" );
				$stderr.printf( "%s [%d] %s\n", Ng_time.now( Ng_time::COMPRESSED ), level, ustr );
			end
		end
	end

	# ----------- public ------------------------------------
	def initialize( id )
		if( id != nil )
			@id = id;		# id for the message
		else
			@id = "unidentified";
		end

		@logname = "MASTER_LOG"
		@seqnum = 1;
		@pid = Process::pid();
		@pri_txt = Array.[]( "000[EMERG]", 
			"001[ALERT]", 
			"002[CRIT]", 
			"003[ERROR]", 
			"004[WARN]", 
			"005[NOTICE]", 
			"006[INFO]" );

		@node = IO.popen( "ng_sysname", "r" ) { |f| f.gets().chomp!() };

		@logger_port = Cartulary.get( "NG_LOGGER_PORT" );
		@log_if = Cartulary.get( "LOG_IF_NAME" );		# interface where log messages are sent
		@nack_log = Cartulary.get( "LOG_NACKLOG" );		# where we log messages that did not receive enough acks
		@ack_needed = Cartulary.get( "LOG_ACK_NEEDED" ).to_i();
		if( @ack_needed == nil )
			@ack_needed = 1;
		end

		@bcast = Broadcast.new( nil, @logger_port, @log_if );			# create and open a new broadcast object
	end

	# do the real work of formatting and sending the message.
	def fmt_send( priority, fmt, args=nil )
		i = 0;

		# build the user string
		fmt.gsub!( "\n", " " );

		if( args == nil )
			ustr = fmt;
		else
			ustr = fmt.gsub( /%[0-9]*[.]*[0-9]*[dflsx%]/ ) do |m| 
				if( i < args.length() )
					i += 1;				# must incr first so this does not become the return value
					sprintf( m, args[i-1] );
				end
			end
		end

		# format the log message with timestamp, priority and other goo
		# if result is bigger than what will fit into a udp packet; truncate it before sending
		lmsg = sprintf( "%04d %s; %s %s %s %s[%d] %s", @seqnum, @logname, Ng_time::now( Ng_time::TIMESTAMP ), @pri_txt[priority], @node, @id, @pid, ustr );
		if( lmsg.size() > 1440 )
			lmsg = lmsg[0,1440] + "<trunc>";
		end
		
		#$stderr.printf( "send message to localhost:%s (%s)\n", @logger_port, lmsg );
		@bcast.send( lmsg );
		

		# wait for responses
		i = 0;
		while( i < @ack_needed )
			if( (response = @bcast.recv( 50, 2 )) != nil )
				if( (rs = response[1..-1].to_i() ) == @seqnum )
					#$stderr.printf( "ng_log: got response %s\n", response );
					i += 1;
				#else
				#	$stderr.printf( "ng_log: IGNORED response %s\n", response );
				end
			else
				$stderr.printf( "not enough acks received to broadcast (wanted=%d got=%d) msg\n", @ack_needed, i  );
				nfile = File.open( @nack_log, "a" );
				nfile.printf( "%s\n", lmsg );
				nfile.close( );
				break;
			end
	 	end	

		# incr sequence after we get responses so we can match number expected
		if( (@seqnum += 1) > 9999 )
			seqnum = 1;		# must wrap to 1 not 0!
		end
	end
end

#nl = Ng_log.new( "ng_log_test" );
#Ng_log.printf( Ng_log.ERROR, "hello fred the process failed rc=%d\n", 7.056 );
#Ng_log.printf( Ng_log.WARN, "this is a warning process rc=%s\n", "some string for message" );
#Ng_log.printf( Ng_log.INFO, "JUST TESTING an information message %f %d \n", 7.3489, 7.234 );
#Ng_log.printf( Ng_log.INFO, "JUST TESTING an second information message %f %d \n", 7.3489, 7.234 );
#Ng_log.printf( Ng_log.CRIT, "crit message with no arguments" );


#$verbose = $*[0].to_i();
#if( $verbose == nil )
#	verbose -= 0;
#end
#
#$stderr.printf( "testing starts: v=%d\n", $verbose );
#Ng_log.bleat( 0, "bleat message test -- always shown (verbose=%d)\n", $verbose );
#Ng_log.bleat( 1, "bleat message test verbose level 1 (verbose=%d)\n", $verbose );
#Ng_log.bleat( 11, "bleat message test only when lib cat (10) and verbose >= 1 set (verbose=%d)", $verbose );
#Ng_log.bleat( 20, "bleat message test only when network cat (20) but for all verbose values  (verbose=%d)", $verbose );
#Ng_log.bleat( 21, "bleat message test only when network cat (20) and verbose >= 1 set (verbose=%d)", $verbose );
#Ng_log.bleat( 31, "bleat message test  when lib or network cat (20) and verbose >= 1 set (verbose=%d)", $verbose );


=begin
#---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_log.rb:Interface to Ningaui log environment)

&desc 
	The ng_log class provides a program the ability to generate messages 
	that are recorded into the ningaui master log on all nodes in the cluster. 
	
&cconst
	These prirority constants are available and should be used to indicate the 
	message priority:
&begterms
&term	CRIT :  Message type is critical. External processes may recognise critical errors
	and cause external alerts (emails, text messages, pages) to be dispatched.
&space
&term	ERROR : Message describes an error -- the programme encountered a state or condition 
	that it could not recover from. 
&space
&term	 WARN : Message describes a warning -- the programme encoutered a condition that it 
	was prepared to take alternate, but less desired, action to allow the programme to continue.
&space
&term	INFO : Informational message.
&endterms

&cmeth
&subcat printf( priority, format_str, ... )
	The printf class method provides the interface for sending a message to the masterlog.
	The format string is a &lit(printf) format string which may be followed by one or more 
	optional arguments. Each message written will be prefixed with a date stamp, standard
	priority indicator (e.g. 006[INFO]), and the name of the programme (assuming that $argv0 
	was set). 
&space
	For any ruby programme that needs to write to the ningaui masterlog, there need
	be only one instance ot the ng_log object.  Using the class &lit(printf) method, rather
	than the instance &lit(_printf) method will ensure that only one object exists and will
	make the log interface easier for the application programme.

&subcat bleat( vlevel, forat_str, ... )
	Format and write the resulting string using &lit(printf) style formatting. The resulting 
	message is written to the standard error device, but only if the value of the &lit($verbose) 
	global variable is greater or equal to the &lit(vlevel) passed in.  
	After the user message is formatted, a
	a compressed timestamp (yyyymmddhhmmss) and the message level are prepended to the text. 
&space
	If &lit(vlevel) is greater than 10, then the integer resulting from the computation &lit(vlevel/10)
	is used as a mask against the current value of &lit($verbose) after it is divided by 10. The
	mask serves as a catigory mask allowing a verbose level of 2 to apply to library objects
	only if the library mask bit is set.  The mask values are application specified, but the 
	suggested values are: 
&space
&beglist
&item	0x01 : Library object/methods
&item	0x02 : Network object/methods
&endlist

&space
	Masking also allows for a message to be written when the catigory is set to either 
	library or network mode. As an example, the following message would be written only 
	when verbose level 2 and the networking mask was set:
&space
&litblkb
   Ng_log.bleat( 22, "unable to connect to host: %s", hname );
&litblke


&space
&construct
&subcat	new( id )
	Creates and initialises the object. The &lit(id) parameter is used to identify the source
	of the message in the master log.  Typically, this is the name of the binary that was invoked.
	If &ital(id) is &lit(nil,) then &this will attempt to determine the name of the programm
	(using the contents of $_argv0 if set). 
&space
&imeth	
	There need be only one instance of the logger object per invocation of a programme, so 
	there should be no need for the user to create the object. Using the class method 
	described above (see examples below) the use of the constructor can be avoided.
	
&space
&subcat	fmt_send( priority, format_str, args_array )
	Formats and sends a message to the ningaui logger daemon. Messages are broadcast via UDP
	to all other nodes on the local network, and thus message lengths might be truncated
	by this method, or underlying network transmission software.  This method imposes a 
	length of 1440 characters (including the header added by this method)

&space
&examp
	These statements illustrate how this object can be used to send messages to the masterlog.
&litblkb
  Ng_log.printf( Ng_log::WARN, "warning ec=%d", error_count );
  Ng_log.printf( Ng_log::INFO, "JOB_TRACE new %s", job_name );
  Ng_log.printf( Ng_log::CRIT, "filesystem not mounted: %s", fsname );
&litblke

&space	These statements cause messages to be written to the stderr device:

&litblkb
  Ng_log.bleat( 1, "message written if verbose level > 0" );
  Ng_log.bleat( 0, "message always written " );
  Ng_log.bleat( 10, "message always written when the library flag is set" );
  Ng_log.bleat( 21, "written only when the network flag is set and vlevel > 0 " );
  Ng_log.bleat( 32, "written only when either the network or lib flag is set and vlevel > 1 " );
&litblke

&space
&see	&seelink(tool:ng_log) &seelink(lib:ng_log) &seelink(lib:ng_bleat)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	15 Jan 2008 (sd) : Its beginning of time. 
&endterms


&scd_end
=end

