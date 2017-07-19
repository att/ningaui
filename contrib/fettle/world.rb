UNIT_none = 0
UNIT_u = 1
UNIT_rack = 2
UNIT_inch = 3

DEFN_BOX = 1
DEFN_RACK = 2

ANCHOR = 'anchor'

class Obj
	def initialize(name, desc, dim, vrml, ports)
		@name = name
		@desc = desc
		@dim = dim
		@vrml = vrml
		@ports = ports
	end
	def to_s
		"((#{@name}: #{@desc}))"
	end
	def dim
		@dim
	end
	def vrml
		@vrml
	end
	def ports
		@ports
	end
	def port_by_name(p)
		@ports.each{ |pp|
			if pp.name == p
				return pp
			end
		}
		return nil
	end
	def canon
		if @ports
			@ports.each{ |p|
				p.canon(@dim)
			}
		end
	end
end

class Rack
	def initialize(name, desc, dim, doors, bars, ports)
		@name = name
		@desc = desc
		@dim = dim
		@doors = doors
		@bars = bars
		@ports = ports
	end
	def to_s
		"((#{@name}: #{@desc}))"
	end
	def dim
		@dim
	end
	def ports
		@ports
	end
	def each_door
		@doors.each{ |d|
			yield d
		}
	end
	def port_by_name(p)
		@ports.each{ |pp|
			if pp.name == p
				return pp
			end
		}
		return nil
	end
	def access(sig, o)
		@ports.each{ |pp|
			if (pp.signal == sig) || (pp.signal == nil)
				yield o.add(pp.dim.convert3)
			end
		}
	end
	def barset(wmap, wt)
		if wmap == nil
			wmap = Hash.new
			wmap[wt] = wt
		end
		s = Array.new
		if wt == nil
			@bars.each{ |b|
				s << b
			}
		else
			some = false
			@bars.each{ |b|
				if (b.wtype == wmap[wt]) || (b.wtype == wt)
					s << b
					some = true
				end
			}
			if !some
				@bars.each{ |b|
					if b.wtype == nil
						s << b
						some = true
					end
				}
			end
		end
		s
	end
	def canon
		if @ports
			@ports.each{ |p|
				p.canon(@dim)
			}
		end
	end
end

class Port
	def initialize(name, dim, signal, sink)
		@name = name
		@dim = dim
		@signal = signal
		@sink = sink
	end
	def name
		@name
	end
	def sink
		@sink
	end
	def dim
		@dim
	end
	def signal
		@signal
	end
	def canon(d)
		@dim.canon(d)
	end
end

class Door
	def initialize(dim1, dim2, vrml)
		@origin = dim1
		@dim = dim2
		@vrml = vrml
	end
	def to_s
		"door(#{@origin} -- #{@dim})"
	end
	def origin
		@origin
	end
	def dim
		@dim
	end
	def vrml
		@vrml
	end
	def canon(d)
		@origin.canon(d)
	end
end

class Bar
	def initialize(nm, dim1, dim2, wtype)
		@name = nm
		@origin = dim1
		@dim = dim2.convert3.add(dim1.convert3.neg)
		@wtype = wtype
	end
	def to_s
		"bar(#{@name}: #{@origin} -- #{@dim})"
	end
	def name
		@name
	end
	def origin
		@origin
	end
	def dim
		@dim
	end
	def wtype
		@wtype
	end
	def canon(d)
		@origin.canon(d)
	end
	def offset(o)
		@origin = @origin.convert3.add(o)
	end
end

class Coord
	def initialize(s, v, u)
		@sign = s
		@val = v
		@unit = u
	end
	def to_s
		"(#{@sign}:#{@val}:#{@unit})"
	end
	def sign
		@sign
	end
	def val
		@val
	end
	def unit
		@unit
	end
	def inches
		x = case @unit
		when UNIT_inch	then 1
		when UNIT_u		then 1.75
		when UNIT_rack	then 17
		else			 1
		end * @val
		if @sign < 0
			-x
		else
			x
		end
	end
	def add(c)
		if @unit == UNIT_none
			@unit = c.unit
		end
		if (@unit == c.unit) || (c.unit == UNIT_none)
			x = @val * (@sign < 0? -1:1) + c.val * (c.sign < 0? -1:1)
			@sign = 0
			@val = x
		else
			x = self.inches + c.inches
			@sign = 1
			@unit = UNIT_inch
			@val = x
		end
	end
	def canon(c)
		if @sign < 0
			add(c)
		end
	end
end

class Coord3
	def initialize(x, y, z)
		@x = x
		@y = y
		@z = z
	end
	def to_s
		"(#{@x}, #{@y}, #{@z})"
	end
	def x
		@x
	end
	def y
		@y
	end
	def z
		@z
	end
	def add(c3)
		Coord3.new(@x+c3.x, @y+c3.y, @z+c3.z)
	end
	def sub(c3)
		Coord3.new(@x-c3.x, @y-c3.y, @z-c3.z)
	end
	def neg
		Coord3.new(-@x, -@y, -@z)
	end
	def scale(f)
		Coord3.new(@x*f, @y*f, @z*f)
	end
	def dot(c3)
		@x*c3.x + @y*c3.y + @z*c3.z
	end
	def dist(c3)
		Math.sqrt((@x-c3.x)*(@x-c3.x) +
			(@y-c3.y)*(@y-c3.y) + (@z-c3.z)*(@z-c3.z))
	end
	def zero
		(@x.abs < EPSILON) && (@y.abs < EPSILON) && (@z.abs <  EPSILON)
	end
	def xzero
		(@x.abs < EPSILON)
	end
	def yzero
		(@y.abs < EPSILON)
	end
	def zzero
		(@z.abs <  EPSILON)
	end
	def norm
		d = Math.sqrt(@x*@x + @y*@y + @z*@z)
		if d == 0
			d = EPSILON
		end
		@x = @x/d
		@y = @y/d
		@z = @z/d
		self
	end
end

class Dim
	def initialize(w, h, d)
		@w = w
		@h = h
		@d = d
	end
	def to_s
		"[#{@w}, #{@h}, #{@d}]"
	end
	def w
		@w
	end
	def h
		@h
	end
	def d
		@d
	end
	def add(d)
		@w.add(d.w)
		@h.add(d.h)
		@d.add(d.d)
	end
	def canon(d)
		@w.canon(d.w)
		@h.canon(d.h)
		@d.canon(d.d)
	end
	def convert3
		Coord3.new(@w.inches, @h.inches, @d.inches)
	end
end

class Inst
	def initialize(ndefn, name)
		@ndefn = ndefn
		@defn = nil
		@name = name
		@norigin = nil
		@origin = nil
		@dim = nil
		@pos = nil
		@ports = Hash.new
		@bounds = nil
		@gbars = nil
		@mybars = Array.new
	end
	def to_s
		"(#{@name}:#{@ndefn})"
	end
	def name
		@name
	end
	def ndefn
		@ndefn
	end
	def defn
		@defn
	end
	def norigin
		@norigin
	end
	def origin
		@origin
	end
	def pos
		@pos
	end
	def dim
		@dim
	end
	def i1
		@i1
	end
	def i2
		@i2
	end
	def gbars
		@gbars
	end
	def set_gbars(gb)
		@gbars = gb
	end
	def set_defn(d)
		@defn = d
	end
	def set_origin(o)
		@origin = o
	end
	def attach(o, d)
		@norigin = o
		@dim = d
	end
	def place(d)
		@pos = d
		@i1 = d.convert3
		@i2 = @i1.add(@defn.dim.convert3)
#puts "place(#{@name} at #{@pos} defn=#{@defn.dim} i1=#{@i1} i2=#{@i2}"
	end
	def intersect(i)
		notouch = (@i1.x >= i.i2.x) || (i.i1.x >= @i2.x) ||
			(@i1.y >= i.i2.y) || (i.i1.y >= @i2.y) ||
			(@i1.z >= i.i2.z) || (i.i1.z >= @i2.z)
		!notouch
	end
	def connect(port)
		if @ports[port]
			sem_error("multiple connections to #{@name}.#{port}")
		end
		@ports[port] = true
	end
	def connected(port)
		@ports[port]
	end
	def access(sig)
		@defn.access(sig, @pos.convert3)
	end
	def addbar(b)
		@mybars << b
	end
	def mybarset(wmap, wt)
		s = Array.new
		if wmap == nil
			wmap = Hash.new
			wmap[wt] = wt
		end
		if wt == nil
			@mybars.each{ |b|
				s << b
			}
		else
			some = false
			@mybars.each{ |b|
				if (b.wtype == wmap[wt]) || (b.wtype == wt)
					s << b
					some = true
				end
			}
			if !some
				@mybars.each{ |b|
					if b.wtype == nil
						s << b
						some = true
					end
				}
			end
		end
		s
	end
	def barset(wmap, wtype)
		b1 = defn.barset(wmap, wtype)
		b2 = Array.new
		b1.each{ |b|
			b = b.clone
			b.offset(@i1)
			b2 << b
		}
		mybarset(wmap, wtype).each{ |b|
			b = b.clone
			b.offset(@i1)
			b2 << b
		}
		b2
	end
end

class Wire
	def initialize(nm, src, sink, wtype)
		@name = nm
		@src = src
		@sink = sink
		@wtype = wtype
		@vrml = nil
	end
	def to_s
		"wire(#{@src} -> #{@sink})"
	end
	def src
		@src
	end
	def sink
		@sink
	end
	def name
		@name
	end
	def vrml
		@vrml
	end
	def canon(defns)
		if @wtype == nil
			return true
		end
		if (d = defns[@wtype])
			@vrml = d.vrml
			return true
		else
			sem_error("unknown wiretype #{@wtype} for wire #{@name}")
			return false
		end
	end
	def wtype
		@wtype
	end
	def terms
		yield @src
		yield @sink
	end
end

class Wiretype
	def initialize(name, desc, sig, vrml)
		@name = name
		@desc = desc
		@sig = sig
		@vrml = vrml
	end
	def name
		@name
	end
	def vrml
		@vrml
	end
	def sig
		@sig
	end
end

class Terminal
	def initialize(inst, port)
		@inst = inst
		@port = port
	end
	def to_s
		"#{@inst}.#{@port}"
	end
	def inst
		@inst
	end
	def port
		@port
	end
end

class Anchor
	def initialize(inst, dim)
		@inst = inst
		@dim = dim
	end
	def to_s
		"anchor #{@inst} to #{@dim}"
	end
	def inst
		@inst
	end
	def dim
		@dim
	end
end

class Routed
	def initialize(p, b)
		@p = p
		@bar = b
	end
	def p
		@p
	end
	def bar
		@bar
	end
end

