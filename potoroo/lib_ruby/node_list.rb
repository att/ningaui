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



#!/usr/bin/env ruby
# Mnemonic:	node_list.rb
# Abstract:	class to build and manage a list of nodes. provides a by name hash
#		to allow user to map a data object to each node
#		
# Date:		24 March 2008
# Author:	E. Scott Daniels
# ---------------------------------------------------------------------------------


class Node_list
	def initialize( cluster=nil )
		@list = "";
		@tokens = nil;

		if( cluster == nil )
			f = IO.popen( "ng_members " )	# for current cluster 
		else
			f = IO.popen( "ng_members -c #{cluster}" )	# for named cluster 
			
		end

		while( (r  = f.gets( )) != nil  ) 
			@list = r.chomp!();
		end
		f.close( );
	
		@tokens = @list.split();
		@nodes = Hash.new( nil );
	end

	# number of nodes in the cluster
	def length()
		return @tokens == nil ? 0 : @tokens.length();
	end

	def size()
		return @tokens == nil ? 0 : @tokens.length();
	end

	def add_node( n, data=nil )
		if( @list == "" )
			@list = n;
		else
			@list += " " + n;
		end

		@tokens << n;

		if( data != nil )
			@nodes[nname] = data;
		end
	end

	def get_list()
		return @list;
	end

	# user block iterates once per node name
	def each_name()
		@tokens.each() {|t| yield(t); }
	end

	# user block iterates once per node name/data pair
	def each_data()
		@tokens.each() {|t| yield( t, @nodes[t] ); }
	end

	# allows user to set data for a specific node (probably pointer to object)
	def set_data( nname, data )
		@nodes[nname] = data;
	end

	# get the data that has been associated with a particular node
	def get_data( nname )
		return @nodes[nname];
	end
end


=begin
# -------------- self testing ---------------------------
class Node_data
	def initialize( jobs=0, files=0 )
		@jobs = jobs;
		@files = files;
	end

	def set_jobs( j )
		@jobs = j;
	end

	def set_files( f )
		@files = f;
	end

	def get_jobs( )
		return @jobs;
	end
	
	def get_files
		return @files;
	end
end


cluster = nil;
if( $*.length() >= 1 )
	cluster = $*[0];
end
n = Node_list.new( cluster );
if( n == nil )
	$stdout.printf( "unable to get list of members for %s\n", cluster );
	exit( 1 );
end
	$stdout.printf( "unable to get list of members for %s\n", n );

jobs = 1;
files = 2;
$stdout.printf( "members = %d\n", n.size() );
n.each_name() { |nm| 
	$stdout.printf( "%s\n", nm ); 
	nd = Node_data.new( jobs, files );
	n.set_data( nm, nd );
	jobs += 1;
	files += 3;
}
n.each_data() { |nm, nd| $stdout.printf( "%s jobs=%d files=%d\n", nm, nd.get_jobs(), nd.get_files() ); }
=end


=begin
#---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(Node_list.rb:Create and manage a list of nodes)

&desc 
	The &this class creates a list of nodes (using ng_members) and allows the user 
	to associate a data object with any/all nodes.  Also provides iteration methods
	that allow a user block to be driven for each node name, or each name, data pair
	that is known.
	
.** class constants
&cconst
&begterms
&endterms

.** class methods
.** &space
.** &cmeth
.** &subcat method(args)
	

.** constructor(s)
&space
&construct
&subcat	new( cluster_name=nil )
&space
	Creates the object using the result from &lit(ng_members) for the current cluster. 
	A list for another cluster can be managed if cluster_name is passed in. 
	An empty list, the user intends on adding nodes with the add_node() method, 
	can be created by providing the name of a non-existant cluster. 

.** instance methods
&space
&imeth	
&subcat	 length() => int
	Returns the number of nodes in the managed list.

&subcat	 size() => int
	Returns the number of nodes in the managed list.

&subcat	 add_node( n, data=nil ) 
	Adds a node to the list. Data is added if supplied. This allows the user to 
	build a special list of nodes.  

&subcat	 get_list() => String
	Return a string which contains all of the node names. 

&subcat	 each_name() {|name| block} 
	Drivers the user block for each name in the list. 

&subcat	 each_data() {|name,data| block}
	Drives the user block for each name,data pair in the hash.  Oder is not known or 
	guarenteed to be consistant from one call to the next.

&subcat	 set_data( nname, data ) 
	Assignes the data object to the named node in the list. This assumes that the node 
	has been added to the list via the construtor (provided with a legitimate cluster
	name), or using the add_node() method. 

&subcat	 get_data( nname ) => object
	Retrieves the data that has been associated with the node name given as the argument.

	
&see
	&seelink(tool:ng_members)

&space
&mods
&owner(Scott Daniels)
&lgterms
&term	24 Mar 2008 (sd) : Its beginning of time. 
&endterms


&scd_end
=end
