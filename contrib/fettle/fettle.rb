#!/usr/bin/ruby

require 'world'
require 'parse'
require 'check'
require 'route'
require 'vrml'

t = Token.new
t.next
things = Hash.new
defns = Hash.new

parse(t, defns, things)
if t.nerrors > 0
	puts "#{t.nerrors} parse errors; exiting"
	exit(1)
end

if !post_parse(defns, things)
	puts "semantic parse errors found; exiting"
	exit(1)
end

if !check_phys(defns, things)
	puts "failed physical consistency checks; exiting"
	exit(1)
end

if !check_wire(defns, things)
	puts "failed wiring consistency checks; exiting"
	exit(1)
end

cables = Cable.new(defns)
wire_route(defns, things, cables)
wire_summary(defns, things, cables)

vrml_output(defns, things, cables)
