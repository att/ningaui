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
# Mnemonic:	seneschal_if.rb
# Abstract:	interface class to seneschal
#		requiers 1.8.5 or greater to use the non-blocking feature
#	
#		execute seneschal_if_test.rb in this directory to test. 
#		
# Date:		19 May 2009
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------


require( "socket" );
include Socket::Constants

require( "cartulary" );
require( "ng_log" );


class Seneschal_job
	def initialize( jname, ijob, inlist, outlist, user )
		@jname = jname;
		@ijob = ijob;
		@inlist = inlist;
		@outlist = outlist;
		@user = user;
	end

	def submit( socket )
		socket.write( @ijob );

		if( @inlist != nil )
			0.upto(@inlist.length() -1) { |i|
				s = sprintf( "input=%s:no_md5,%s\n", @jname, @inlist[i] );   # seneschal doesn't yet support md5, but needs the place holder
				socket.write( s );
			}
		end

		if( @outlist != nil )
			0.upto(@outlist.length() -1) { |i|
				s = sprintf( "output=%s:%s\n", @jname, @outlist[i] );
				socket.write( s );
			}
		end
		socket.write( "user=" + @jname + ":" + @user +  "\n" );
		socket.write( "cjob=" + @jname + "\n" );
	end

	def print( )
		$stdout.printf( "job: %s user=%s %s", @jname, @user, @ijob ); 	# cmd has a \n for seneschal, so no need for one here
		if( @inlist != nil )
			0.upto(@inlist.length() -1) { |i|
				$stdout.printf( "job: input: %s\n",  @inlist[i] );
			}
		end
		if( @outlist != nil )
			0.upto(@outlist.length() -1) { |i|
				$stdout.printf( "job: output: %s\n",  @outlist[i] );
			}
		end
	end
end

# each instance starts a 'logical' connection to seneshal only one real tcp session 
# is needed.  jobs submitted from each session are given a name that can be used to 
# map back to the logical session if the user programme wants to track different classes
# of jobs with different 'sessions.'
#
class Seneschal_if

	# ------------------- construction -----------------------------------------------------
	def initialize( name, host=nil, port=nil )
		@conn_name = nil;		# connection name given to seneschal (e.g. daemon name like nawab or collate)
		@sene_host = nil;
		@sene_port = nil;
		@socket = nil;
		@smsgs = nil;			# set of status messages that were last read
		@smsgs_idx = -1;
		@have_session = false;
		@user = "nobody";
		@job_list = Hash.new( nil );

		@conn_name = name;

		if( host != nil )
			@sene_host = host;	# always use the user supplied name
		end

		if( port != nil )
			@sene_port = port;	# always use the user supplied port
		end

		# get the user name
		@user = "uid" + Process.uid().to_s;
		conn2sene();
	end

	# --------------------------- private -------------------------------------------------
	private
	def resub( )
		if( @job_list == nil )
			return;
		end

		@job_list.each_pair( ) do |jname,job|
			Ng_log.bleat( 11, "sene_if: resub: %s\n", jname );
			job.submit( @socket );
		end
	end

	def conn2sene( )
		Ng_log.bleat( 12, "sene_if: connecting to seneschal....." );

		if( @sene_host != nil )
			host = @sene_host;			# use user supplied value on each connection 
		else
			host = Cartulary::get( "srv_Jobber" );	# use cart value if user omitted host and/or port on construction
			Ng_log.bleat( 12, "sene_if: host from cartulary=%s...", host );
		end

		if( @sene_port != nil )
			port = @sene_port; 
		else
			port = Cartulary::get( "NG_SENESCHAL_PORT" );
			Ng_log.bleat( 12, "sene_if: port from cartulary=%d...", port );
		end

		Ng_log.bleat( 11, "sene_if: connect to %s:%d...", host, port );

		begin
			@socket = Socket.new( AF_INET, SOCK_STREAM, 0 )
			sa = Socket.pack_sockaddr_in( port, host );
			@socket.connect( sa );
	
			if( @conn_name != nil )
				@socket.write( "name=" + @conn_name + "\n" );
			end


			rescue
				Ng_log.bleat( 12, "sene_if: connection failed (%s:%d)\n", host, port );
				return false;
		end

		@have_session = true;
		Ng_log.bleat( 12, "sene_if: connected\n" );
		resub( );
		return true;
	end

	# ---------------------------- public -------------------------------------------------
	public

	# close theh session in preperation for the user to trash object
	def close()
		@socket.close();
		@socket = nil;
		@have_session = false;
	end

	# return number of jobs that we've not detected stats for
	def get_job_count()
		return @job_list.length();
	end

	def purge_all_jobs( )
		@job_list.each_key do |jname|
			@socket.write( "purge=" + jname + "\n" );
			@job_list.delete( jname );
		end
	end

	def purge_job( name=nil )
		jname = @conn_name + "." + name; 
		if( (job = @job_list[jname]) != nil )		# it might have gone the way of the doodoo if it completed
			@job_list.delete( jname );
		end
		Ng_log.bleat( 11, "sene_if: job purged: %s", jname );
		@socket.write( "purge=" + jname + "\n" );	# purge even if on complete queue
	end

	# create a job and send it off to seneschal
	# inlist and outlist are  arrays of filenames that match with %i1, %i2... and %o1, %02... in cmd.  nil for either indicates no in/output files to deal with
	# rsrcs can be simple (Rname) or with limit (Rname=2h or Rname=2n)
	# ninf_need is number of input files that are needed to make the job runnable, 0 == all
	# target is the node to run on *==any can also be !nodename or { attr [attr...] } where attr can also be !attr
	#
	def sub_job( name, cmd, target="*", rsrcs="*", inlist=nil, outlist=nil,  pri=20, att=1, size=600, ninf_need=0 )
		if( ! @have_session )
			if( ! conn2sene() )
				return false;
			end
		end

		jname = @conn_name + "." + name; 
		job = Seneschal_job.new( jname, sprintf( "ijob=%s:%s:%s:%s:%s:%s:%s:*:*:cmd %s\n", jname, att, size, pri, rsrcs, target, ninf_need, cmd ), inlist, outlist, @user );
		job.submit( @socket );

		@job_list[jname] = job;

		return true;
	end

	# read from seneschal and return first status value.  We assume that status messages are newline terminated
	# and might come more than one per 'message'. if block is true, then the function will block until 
	# something is read.
	#
	def read_jstatus( block=true )
		if( ! @have_session )
			if( ! conn2sene() )
				return "";
			end
		end

		@smsgs_idx += 1;
		if( @smsgs == nil ||  @smsgs_idx >= @smsgs.length() )	# no more from last read
			@smsgs = nil;
			@smsgs_idx = 0;
			if( block )
				begin
					data = @socket.recvfrom( 4096 );

					rescue
						@have_session = false;
						Ng_log.bleat( 11, "sene_if: session to seneschal closed (disc?)\n" );
						return ""
				end
			else
     				begin
					data = @socket.recvfrom_nonblock( 4096 );

     					rescue Errno::EAGAIN
						return "";       			# nothing ready 
					rescue
						Ng_log.bleat( 11, "sene_if: socket closed (disc?)\n" );
						@have_session = false;
						return "";
     				end
			end

			if( data == nil  ||  data[0] == "" )		# data should not be nil, but empty string indicates disc
				@have_session = false;
				return "";
			end

			@smsgs = data[0].split( "\n" );
		end

		cname, junk = @smsgs[@smsgs_idx].split( ":", 2 );	# get whole name of completing job

		@job_list.delete( cname );					# we don't auto submit this one 

		junk, rstr = @smsgs[@smsgs_idx].split( ".", 2 );	# split the goo we added to the job name and return rest of string
		return rstr;					# with out the end of string 
	end

	# read as many stauts message as are currently available and returns an array
	# if block is true, this function will block the first read. 
	def read_group( block=true )
		list = Array.new();

		while( (stuff = read_jstatus( block )) != "" )
			list << stuff;
			block = false;
		end	

		return list;
	end

	def list_jobs()
		@job_list.each_pair( ) do |jname,job|
			job.print( );
		end
	end
end



=begin
#---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(ng_seneschal_if.rb:Ruby interface to seneschal)


&desc
	An interface class to allow ruby programmes to submit jobs
	directly to seneschal. 
&space
&construct
        Seneschal_if.new( name, host=nil, port=nil ) ==> sif
&space
	Creates a TCP/IP connection to seneschal though which job submissions
	can be sent, and status messages received.  The name is generally the 
	programme name (e.g. nawab) and is prepended to any job names that 
	are submitted. 
	The &ital(name) argument allows seneschal to return status notification
	for all jobs submitted on the session, and allows the application to 
	restart/reconnect and continue to receive notifications for jobs that were 
	pending when the connection was lost. 
&space
	If &ital(node) and &ital(port) are not supplied, the seneschal port and 
	host are determined by looking up &lit(NG_SENECHAL_PORT) and &lit(srv_Jobber)
	in the cartulary.  In this mode, the session will follow the jobber if it 
	moves nodes. 

&imeth
&subcat	sif.close() ==> nil
	Closes the connection to seneschal in preparation for the object to be destroyed. 
&space
&subcat sif.get_job_count() ==> int
	Returns the number of jobs that have been sent to seneschal for which no status 
	has been received. 

&space
&subcat	sif.list_jobs() ==> nil
	Lists all of the pending jobs to the standard output device. 

&space
&subcat	sif.purge_all_jobs( ) ==> nil
	Purges all of the jobs that have been submitted to seneschal and have not completed. 

&space
&subcat	sif.purge_job( name ) ==> nil
	Purges the named job regardless of its state (completed or not). 

&space
&subcat	sif.read_group( block=true ) ==> Array
	Reads as many job completion status messages as are currently available on the 
	session and rturns an array of Strings. 
	If &ital(block) is true, the method will block until at least one string is 
	available. The method will return an empty list immediately if &ital(block) is false
	and no data is available.
&space
	Each status string contains a colon separated list as returned by seneschal:
&space
&beglist
&item	jobname
&item	node the job executed on 
&item	Return code of the job
&item	Real, system and user time info in the form: 0(real) 0(usr) 0(sys) (values are tenths of seconds).
&endlist
&space
	The data contained in the status message is determined by seneschal and not the interface.
	The time information is not present if the job failed. 	
&space
&subcat	sif.read_jstatus( block=true ) ==> string
	Read a single status message on the queue. The method will block until data is available on the 
	session if &ital(block) is true, otherwise it will return an empty string if no data is immediately
	available. 
&space
	Both of the read methods assume that multiple status messages can be received in the same 
	message and will ensure that one message per string is returned to the user programme. 

&space
&subcat	sif.sub_job( name, cmd, target="*", rsrcs="*", inlist=nil, outlist=nil,  pri=20, att=1, size=600, ninf_need=0 ) ==> true | false
	Causes a job to be submitted to seneschal.  The return value indicates success (true)
	or failure of submission (false). 
	The job is added to a list of pending jobs such that  jobs which have not completed are resubmitted 
	to seneschal if the connection is broken and restarted.  
	
&space
	The followng describes the method's parameters:
&begterms
&term	name : This is the job name that is given to seneschal with the programme name prepended. 
	The name may contain only alphanumeric characters and the underbar (_).  
&space
&term	cmd : The command string that is to be executed. 
&space
&term	target : The node name that the command should be executed on.  The name may also be 
	preceded with a bang (!) to indicate not on the node, or may be a string of space 
	separated attributes enclosed in curly braces (.e.g { !Satellite Tape }). 
&space
&term	inlist : An array of strings. Each string is the basename of a file needed as input 
	for the job.  Files must map to "%i1, %i2..." in the command string if they are to 
	be mapped and supplied to the command as full pathnames. 
	If the command does not use any input files that must be located by seneschal (i.e. the 
	job is not to block on input files) then this value is nil.
&space
&term	outlist : An array of strings. Each string is the basename of a file that is generated
	by the programme.  If the job does not produce any output files that must be known to 
	seneschal, then this parameter is nil.
&space
&term	priority : The job priority.  Should be a value between 10 and 70; the default is 20.
&space
&term	attempts : The number of times seneschal should attempt the job before declaring it a hard failure. 
	The default is once. 
&space
&term	size : A size assignment used when the job does not have any input files for seneschal to 
	compute the cost of a job. (Best to let this default).
&space
&term	ninf_need : The number of input files that must be present on a single node in order for 
	seneschal to consider the job as runnable.  If zero (0) is supplied, then all of the 
	listed files must be present in order for seneschal to start the job. 

&endterms



&space
&see
	&seelink(seneschal:ng_seneschal)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	19 May 2009 (sd) : Its beginning of time. 
&endterms


&scd_end

=end
