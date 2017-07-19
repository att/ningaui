class Cable
	def initialize(defns)
		@wmap = Hash.new
		defns.each_pair{ |k,v|
			if v.class == Wiretype
				@wmap[k] = v.sig
			end
		}
		@all = Hash.new
		@std = { "FC" => [ 1*39.74, 2*39.74, 3*39.74, 5*39.74 ],
			"120V" => [2*12, 3*12, 6*12, 7*12 ],
			"Ethernet" => [ 1*12, 2*12, 3*12, 5*12, 7*12, 10*12, 15*12 ],
			"KVM" => [ 1*12, 3*12, 5*12, 7*12, 10*12, 15*12 ]
		}
	end

	def wmap
		@wmap
	end

	def route(ri, v, sig, b, bc, e, ec)
		if $DEBUG
			puts "wire #{v.name}: #{bc}#{b.name} -- #{ec}#{e.name} #{sig} #{v.wtype}"
		end
		rb = rack_of(b)
		re = rack_of(e)
		ans = Array.new
		n = nil
		if re.name == rb.name
			if $DEBUG
				puts "   within rack #{re.name} #{rb.pos} #{rb.i1} #{rb.i2}"
			end
			n = route1(ri, v.name, rb, v.wtype, bc, ec, rb.i1, rb.i2, ans, @wmap)
			if $DEBUG
				puts "   ===> return #{v.name}[#{sig}]   #{n}  (#{wire_length(ans)}) inch route #{ans}"
			end
			if ans.empty?
				puts "!!!! can't route #{v.name}"
			end
		else
			if $DEBUG
				puts "   cross rack route"
			end
			bestn = nil
			besta = Array.new
			if $DEBUG
				puts "re=#{re}=#{re.class} e=#{e}"
			end
			rb.defn.access('access', rb.i1){ |ba|
			re.defn.access('access', re.i1){ |ea|
			rb.gbars.each{ |bar|
				p1 = closest(ba, bar)
				p2 = closest(ea, bar)
				leg1 = Array.new
				ri.reset
				route1(ri, v.name, rb, v.wtype, bc, p1, rb.i1, rb.i2, leg1, @wmap)
				leg2 = Array.new
				ri.reset
				route1(ri, v.name, re, v.wtype, p2, ec, re.i1, re.i2, leg2, @wmap)
				s1, mf1, tf1 = route_string(leg1)
				n1 = wire_length(leg1)
				s2, mf2, tf2 = route_string(leg2)
				n2 = wire_length(leg2)
				if $DEBUG
					puts "ba=#{ba} ea=#{ea} bar=#{bar}: p1=#{p1} p2=#{p2} n=#{n1},#{n2} tf=#{tf1} #{tf2} legs=|#{s1}| |#{s2}|"
				end
				tf = tf1+tf2
				if (n1 != 0) && (n2 != 0) && ((bestn == nil) || (tf < bestn))
					if $DEBUG
						puts "new best: #{tf}"
					end
					bestn = tf
					besta = leg1 + leg2
				end
			}}}
			if $DEBUG
				puts "   ===> return #{v.name}[#{sig}]  #{bestn} (#{wire_length(besta)} inch route #{besta}"
			end
			n = bestn
			ans = besta
			if ans.empty?
				puts "!!!! can't route #{v.name}"
			end
		end
		if @all[v.wtype] == nil
			@all[v.wtype] = Hash.new
		end
		@all[v.wtype][v.name] = ans
		s, mf, tf = route_string(ans)
		STDOUT.printf("%s: len=%.2fin totf=%.2fin %s\n", v.name, wire_length(ans), tf, s)
	end

	def wire(name)
		@all.each{ |k,a|
			if @all[k][name] != nil
				return @all[k][name]
			end
		}
		nil
	end

	def summary(defns)
		nall = 0
		nr = 0
		@all.each{ |k,a|
			cnt = Array.new
			nweird = 0
			a.each{ |j,b|
				nall += 1
				if !b.empty?
					nr += 1
				end
				x = wire_length(b)+12
				found = nil
				ki = defns[k].sig
				@std[ki].each_index{ |i|
					if x <= @std[ki][i]
						found = i
						break
					end
				}
				if found == nil
					nweird = nweird+1
					puts "warning: #{k} wire #{j} has length #{x} inches"
				else
					if cnt[found] == nil
						cnt[found] = 1
					else
						cnt[found] = cnt[found]+1
					end
				end
			}
			puts "#{k} cable:"
			cnt.each_index{ |i|
				$stdout.printf("\t%s: %d\n", @std[defns[k].sig][i], cnt[i])
			}
			puts "\tweird: #{nweird}"
		}
		puts "\n#{nall} wires routed; #{nall-nr} failed"
	end
end

class Rinfo
	def initialize
		@n = 0
	end
	def reset
		@lim = nil
	end
	def inc_alt
		@n += 1
	end
	def n_alt
		@n
	end
	def set_lim(x)
		@lim = x
	end
	def lim
		@lim
	end
	def too_big(x)
		(@lim != nil) && (x >= @lim)
	end
end

def rack_of(i)
	while i.defn.class != Rack
		i = i.origin
	end
	i
end

def wire_length(a)
	n = 0
	c = nil
	a.each{ |p|
		if c != nil
			d = p.p.sub(c)
			n += Math.sqrt(d.dot(d))
		end
		c = p.p
	}
	n
end
	
EPSILON = 0.01

def zero(x)
	x.abs < EPSILON
end

	def clamp(q, b, e)
		e += b
		if q < b
			b
		elsif q > e
			e
		else
			q
		end
	end

def closest(p, bar)
	dim = bar.dim
	if $DEBUG
		puts "=> #{dim} == #{p} == #{bar.origin}"
	end
	x, y, z = bar.origin.x, bar.origin.y, bar.origin.z
	if p.class == Bar
	then
		if $DEBUG
			puts "bar vs bar"
		end
		if p.dim.dot(dim) < EPSILON
			# orthogonal
			if $DEBUG
				puts "-_-_-_ orthog"
			end
			if zero(dim.x) && zero(dim.y)		# z-axis bar
				z = clamp(p.origin.z, bar.origin.z, dim.z)
			elsif zero(dim.x) && zero(dim.z)	# y-axis bar
				y = clamp(p.origin.y, bar.origin.y, dim.y)
			elsif zero(dim.y) && zero(dim.z)	# x-axis bar
				x = clamp(p.origin.x, bar.origin.x, dim.x)
			else
				puts "!!!!! skew bar #{bar}"
			end
		else
			# not overlapping, presumed parallel
			# so just punt
			return closest(p.origin.add(p.dim.scale(0.5)), bar)
		end
	else
		if zero(dim.x) && zero(dim.y)
			z = clamp(p.z, bar.origin.z, dim.z)	# z-axis bar
		elsif zero(dim.x) && zero(dim.z)
			y = clamp(p.y, bar.origin.y, dim.y)	# y-axis bar
		elsif zero(dim.y) && zero(dim.z)
			x = clamp(p.x, bar.origin.x, dim.x)	# x-axis bar
		else
			puts "!!!!! skew bar #{bar}"
		end
	end
	if $DEBUG
		puts "closest(#{p} <> #{bar}) => (#{x},#{y},#{z})"
	end
	Coord3.new(x, y, z)
end

def route_barset(ri, r, b, e, bs, rte, wmap)
	if $DEBUG
		puts "barset: b=<#{b}>  e=<#{e}>  bs=<#{bs}> rte=<#{rte} ri=#{ri.lim}>"
	end
	x1, x2, tf = route_string(rte)
	if ri.too_big(tf)
		if $DEBUG
			puts "+-+-+-+-+-+- too big"
		end
		return
	end
	# first, try just going straight there
	yield rte+[Routed.new(e, p2bar(r, e, wmap))]	# check here?
	# next, try going down the bar and go there
	if b.class == Bar
		p = closest(e, b)
		if $DEBUG
			puts "----- closest(#{e}, #{b}) yielded #{p}"
		end
		yield rte + [Routed.new(p, b)] + [Routed.new(e, p2bar(r, e, wmap))]
	end
	# next recurse
	bs.each{ |bar|
		nrte = rte.clone
		if b.class == Bar
			p = closest(bar, b)
			nrte << Routed.new(p, b)
			p1 = closest(p, bar)
			nrte << Routed.new(p1, bar)
			if $DEBUG
				puts "~~~~<#{b}> vs <#{bar}> yields #{p} #{p1}"
			end
		else
			p = closest(b, bar)
			nrte << Routed.new(p, bar)
		end
		route_barset(ri, r, bar, e, bs-[bar], nrte, wmap){ |rr|
			yield rr
		}
	}
end

def route1(ri, name, r, sig, b, e, br1, br2, ans, wmap)
	na = 0
	if $DEBUG
		puts "  --> #{r}   ==="
	end
	xx = r.barset(wmap, sig)
	if $DEBUG
		puts "      -> barset(#{sig}): #{xx}   ==="
	end
	bestfree = nil
	bestlen = 0
	best = Array.new
	route_barset(ri, r, b, e, xx, [Routed.new(b, p2bar(r, b, wmap))], wmap){ |as|
		ri.inc_alt
		na += 1
		s, maxfree, totfree = route_string(as)
		if $DEBUG
			puts " ===#{na}=> #{wire_length(as)}  #{s} #{maxfree} #{totfree}"
		end
		x = wire_length(as)
		if ((bestfree == nil) || (totfree < bestfree))
			if $DEBUG
				puts "+++++++++ new best #{bestfree} #{bestlen} #{maxfree}"
			end
			best = as.clone
			bestfree = totfree
			bestlen = x
			ri.set_lim(totfree)
		end
	}
	best.each{ | p|
		ans << p
	}
	bestlen
end

def wire_route(defns, things, cables)
	ri = Rinfo.new
	todo = 100000
#todo = 1
	things.each{ |k,v|
#v = things['nxx06_kvm__kvm1_p07']
		if v.class == Wire
			b1 = nil
			b2 = nil
			c1 = nil
			c2 = nil
			sig = nil
			v.terms { |t|
				d = things[t.inst]
				p = d.defn.port_by_name(t.port)
				sig = p.signal
				c = d.pos.convert3
				c = c.add(p.dim.convert3)
				if b1 == nil
					b1 = d
					c1 = c
				else
					b2 = d
					c2 = c
				end
			}
			ri.reset
			cables.route(ri, v, sig, b1, c1, b2, c2)
			todo = todo-1
			if todo <= 0
				break
			end
		end
	}
	puts "info: #{ri.n_alt}"
end

def p2bar(r, p, wmap)
	r.barset(wmap, nil).each{ |b|
		lp = p.sub(b.origin).norm
		rp = p.sub(b.origin.add(b.dim)).norm
		if lp.zero || rp.zero
			return b
		end
		d = b.dim.clone.norm
		d1 = d.dot(lp)
		d2 = d.dot(rp)
		if d1*d2 == -1
			return b
		end
	}
	nil
end

def route_string(a)
	maxf = -1
	totf = 0
	str = ''
	last = nil
	lbar = nil
	a.each{ |p|
		if last == nil
			str += "#{p.p}"
		else
			d = last.dist(p.p)
			str += sprintf(" [%.2f]", d)
			if p.bar
				str += "#{p.p}:#{p.bar.name}"
			else
				str += "#{p.p}"
			end
			if ((p.bar == nil) && (lbar == nil)) || (p.bar != lbar)
				if d > maxf
					maxf = d
				end
				totf += d
			end
		end
		lbar = p.bar
		last = p.p
	}
	str += sprintf(" tf=%.2f", totf)
	[str, maxf, totf]
end

def wire_summary(defns, things, cables)
	cables.summary(defns)
end
