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
# Mnemonic:	broadcast.rb 
# Abstract:	creates a broadcast object that can be used by things like ng_log
#		
# Date:		16 January 2008
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------

require "socket"

class Broadcast
	# if the full broadcast IP is not given, we use ifconfig to search for 
	# either the net portion of the IP, or the interface name. We support
	# these styles of ifconfig messages which should cover these flavours:
	# FreeBSD, linux, sun and mac OSX.
        #inet addr:192.168.5.245  Bcast:192.168.5.255  Mask:255.255.255.0
        #inet 192.168.5.244 netmask 0xffffff00 broadcast 192.168.5.255

	def initialize( bcast_net, port, ifname=nil )
		@bcast_ip = nil;
		@port = port;

		if( bcast_net != nil )
			a = bcast_net.split( "." );
		end

		if( a==nil || a.length < 4 )
			IO.popen( "ifconfig #{ifname} |grep inet", "r" ) do |f|	# dig out the broadcast ip address
				while( (r = f.gets()) != nil )
					r.chomp!();
					a = r.split( );
					addr = a[1];
					if( addr[0,5] == "addr:" )
						addr = addr[5..-1];
					end
					if( bcast_net == nil || addr[0,bcast_net.length()] == bcast_net )
						if( a[-2] == "broadcast" )
							@bcast_ip = a[-1];
						else
							@bcast_ip = a[2][6..-1];
						end			
					end
				end
			end
		else
			@bcast_ip = bcast_net;			# if they gave us a full complement
		end

		@sock = UDPSocket.new();   # Socket::AF_INET, Socket::SOCK_DGRAM, 0 );
		@sock.setsockopt( Socket::SOL_SOCKET,  Socket::SO_BROADCAST, 1 );

		#@sock.bind( @bcast_ip, @port );		# if we want to listen on the bcast port
		#a = Socket.sockaddr_in( 4360, "localhost" );
		#s.connect( a );			# does not connnect, but allows send w/o host/port
	end

	def send( message )
		@sock.send( message, 0, @bcast_ip, @port );
	end

	# wait for data (response to bcast on the udp port we used to send, but not bcast messages 
	# since we open a generic port.  This blocks unless a timeout value (seconds) is supplied
	def recv( len, timeout=0 )
		if( select( [@sock], nil, nil, timeout ) == nil )	# wait up to timeout seconds for data to become available
			return nil;
		end

		return @sock.recv( len );
	end
end

#Broadcast.new( nil, 4360, "fxp0").send( "hello world again" );
#Broadcast.new( "135.207", 4360 ).send( "hello world again" );

=begin
#---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(broadcast.rb:UDP Broadcast Object)

&desc 
	The Broadcast class provides an interface for establishing a broadcast UDP socket and
	a method to send a broadcast message.
	
.** constructor(s)
&space
&construct
&subcat	new( bcast_net, port, device_name=nil )
	Causes a new object to be created. &lit(Bcast_net) is assumed to be the network 
	portion of an IP address (e.g. 192.168.0) that is to be used to broadcast on, or 
	the broadcast address itself (all 4 components of the IP address).  If just the 
	network portion is provided, then &lit(ifconfig) is used to suss out the real 
	broadcast address.
&space
	If the device_name is supplied, &lit(bcast_net) is supplied as &lit(nil), then the 
	device name is used when searching through the &lit(ifconfig) output. 

&space
	Setting up a UDP socket is easy; the majority of the work that this object provides
	is sussing out the broadcast address to send messages to, and providing a receive
	method that has a timeout. 

.** instance methods
&space
&imeth	
&subcat	send( message )
	Causes the message to be sent on the established socket. 
	
&space
&subcat recv( length, timeout=0 )
	Wait upto timeout seconds (blocks if not supplied) for data to be received. Returns
	the buffer (at most length bytes) or nil if the timeout expired.  This method is 
	not used to receive broadcast messages, but rather any responses that are sent 
	back to the port that we used to send the broadcast message from.   Some broadcast
	listeners (like the ningaui logger) respond with an ack to the ip:port that sent 
	the message out. 
&space
&examp
&litblkb
  b = Broadcast.new( nil, 4360, "fxp0"); # setup using device name
  b.send( "hello world\n" );

  b = Broadcast.new( "192.168.1", 4360 );	# setup using the network portion of the IP 
  b.send( "hello world\n" );
&litblke

&space
&see	&seelink(lib:ng_netif), &seelink(lib:ng_log), &seelink(lib_ruby:ng_log)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	16 Jan 2008 (sd) : Its beginning of time. 
&endterms


&scd_end
=end
