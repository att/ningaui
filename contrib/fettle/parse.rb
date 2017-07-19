#!/usr/bin/ruby

TOK_LBRACE = 1
TOK_RBRACE = 2
TOK_LBRACK = 3
TOK_RBRACK = 4
TOK_COMMA = 5
TOK_EOF = 6
TOK_LEXERROR = 7
TOK_LPAR = 8
TOK_RPAR = 9
TOK_DOT = 10

TOK_STRING = 15
TOK_NAME = 16
TOK_NUMBER = 17

TOK_object = 20
TOK_rack = 21
TOK_inst = 22
TOK_attach = 23
TOK_wire = 24
TOK_name = 25
TOK_desc = 26
TOK_dim = 27
TOK_ports = 28
TOK_anchor = 29
TOK_sink = 30
TOK_wiretype = 31
TOK_alias = 32
TOK_vrml = 33
TOK_doors = 34
TOK_bars = 35

require 'lex'

nwire = 1

def parse(t, defns, things)
	while true
		case t.op
		when TOK_object
			p_object(t, defns, things)
		when TOK_rack
			p_rack(t, defns, things)
		when TOK_inst
			p_inst(t, defns, things)
		when TOK_attach
			p_attach(t, defns, things)
		when TOK_wire
			p_wire(t, defns, things)
		when TOK_wiretype
			p_wiretype(t, defns, things)
		when TOK_anchor
			p_anchor(t, defns, things)
		when TOK_vrml
			p_vrml(t, defns, things)
		when TOK_bars
			p_bars(t, defns, things)
		when TOK_EOF
			break
		else
			t.error("unexpected token #{t.str}")
			t.next
		end
	end
	return true
end

def p_object(t, defns, things)
	t.next
	if t.op == TOK_LBRACE
		t.next
	else
		t.errtok("{")
	end
	dn = nil
	ddesc = nil
	ddim = nil
	dvrml = nil
	dp = Array.new
	while true
		case t.op
		when TOK_name
			t.next
			if t.op == TOK_STRING
				dn = t.val
				t.next
			else
				t.errtok("string")
			end
		when TOK_desc
			t.next
			if t.op == TOK_STRING
				ddesc = t.val
				t.next
			else
				t.errtok("string")
			end
		when TOK_dim
			t.next
			ddim = p_dim(t, defns, things)
		when TOK_vrml
			t.next
			if t.op == TOK_STRING
				dvrml = t.val
				t.next
			else
				t.errtok("string")
			end
		when TOK_ports
			t.next
			t.next
			while (p = p_port(t, defns, things))
				dp << p
			end
			if t.op == TOK_RBRACE
				t.next
			else
				t.errtok("}")
			end
		when TOK_RBRACE
			t.next
			defns[dn] = Obj.new(dn, ddesc, ddim, dvrml, dp)
			return true
		else
			t.errtok("object field")
		end
	end	
end

def p_dim(t, defns, things)
	if t.op == TOK_LBRACK
		t.next
	else
		t.errtok("[")
		t.next
		return nil
	end
	if t.op == TOK_NUMBER
		w = Coord.new(t.sign, t.val, t.unit)
		t.next
	else
		t.errtok("number")
		t.next
		return nil
	end
	if t.op == TOK_COMMA
		t.next
	else
		t.errtok(",")
		t.next
		return nil
	end
	if t.op == TOK_NUMBER
		h = Coord.new(t.sign, t.val, t.unit)
		t.next
	else
		t.errtok("number")
		t.next
		return nil
	end
	if t.op == TOK_COMMA
		t.next
	else
		t.errtok(",")
		t.next
		return nil
	end
	if t.op == TOK_NUMBER
		d = Coord.new(t.sign, t.val, t.unit)
		t.next
	else
		t.errtok("number")
		t.next
		return nil
	end
	if t.op == TOK_RBRACK
		t.next
	else
		t.errtok("]")
		t.next
		return nil
	end
	return Dim.new(w, h, d)
end

def p_port(t, defns, things)
	if t.op == TOK_LBRACE
		t.next
	else
		return nil
	end
	if t.op == TOK_STRING
		nm = t.val
		t.next
	else
		t.errtok("string")
		t.next
		return nil
	end
	if t.op == TOK_COMMA
		t.next
	else
		t.errtok(",")
		t.next
		return nil
	end
	if (dim = p_dim(t, defns, things)) == nil
		return nil
	end
	if t.op == TOK_COMMA
		t.next
	else
		t.errtok(",")
		t.next
		return nil
	end
	if t.op == TOK_STRING
		signal = t.val
		t.next
	else
		t.errtok("string")
		t.next
		return nil
	end
	if t.op == TOK_sink
		sink = true
		t.next
	else
		sink = false
	end
	if t.op == TOK_RBRACE
		t.next
	else
		t.errtok("}")
		t.next
		return nil
	end
	return Port.new(nm, dim, signal, sink)
end

def p_door(t, defns, things)
	if t.op == TOK_LBRACE
		t.next
	else
		return nil
	end
	if (dim1 = p_dim(t, defns, things)) == nil
		return nil
	end
	if t.op == TOK_COMMA
		t.next
	else
		t.errtok(",")
		t.next
		return nil
	end
	if (dim2 = p_dim(t, defns, things)) == nil
		return nil
	end
	if t.op == TOK_COMMA
		t.next
	else
		t.errtok(",")
		t.next
		return nil
	end
	if t.op == TOK_STRING
		vrml = t.val
		t.next
	else
		t.errtok("string")
		t.next
		return nil
	end
	if t.op == TOK_RBRACE
		t.next
	else
		t.errtok("}")
		t.next
		return nil
	end
	return Door.new(dim1, dim2, vrml)
end

def p_bar(t, defns, things)
	wtype = nil
	if t.op == TOK_LBRACE
		t.next
	else
		return nil
	end
	if t.op == TOK_STRING
		nm = t.val
		t.next
	else
		t.errtok("string")
		t.next
		return nil
	end
	if t.op == TOK_COMMA
		t.next
	else
		t.errtok(",")
		t.next
		return nil
	end
	if (dim1 = p_dim(t, defns, things)) == nil
		return nil
	end
	if t.op == TOK_COMMA
		t.next
	else
		t.errtok(",")
		t.next
		return nil
	end
	if (dim2 = p_dim(t, defns, things)) == nil
		return nil
	end
	if t.op == TOK_COMMA
		t.next
		if t.op == TOK_NAME
			wtype = t.val
			t.next
		else
			t.errtok("name")
			t.next
			return nil
		end
	end
	if t.op == TOK_RBRACE
		t.next
	else
		t.errtok("}")
		t.next
		return nil
	end
	return Bar.new(nm, dim1, dim2, wtype)
end

def p_rack(t, defns, things)
	t.next
	if t.op == TOK_LBRACE
		t.next
	else
		t.errtok("{")
	end
	dn = nil
	ddesc = nil
	ddim = nil
	dp = Array.new
	db = Array.new
	dd = Array.new
	while true
		case t.op
		when TOK_name
			t.next
			if t.op == TOK_STRING
				dn = t.val
				t.next
			else
				t.errtok("string")
			end
		when TOK_desc
			t.next
			if t.op == TOK_STRING
				ddesc = t.val
				t.next
			else
				t.errtok("string")
			end
		when TOK_dim
			t.next
			ddim = p_dim(t, defns, things)
		when TOK_doors
			t.next
			t.next
			while (p = p_door(t, defns, things))
				dd << p
			end
			if t.op == TOK_RBRACE
				t.next
			else
				t.errtok("}")
			end
		when TOK_ports
			t.next
			t.next
			while (p = p_port(t, defns, things))
				dp << p
			end
			if t.op == TOK_RBRACE
				t.next
			else
				t.errtok("}")
			end
		when TOK_bars
			t.next
			t.next
			while (b = p_bar(t, defns, things))
				db << b
			end
			if t.op == TOK_RBRACE
				t.next
			else
				t.errtok("}")
			end
		when TOK_RBRACE
			t.next
			defns[dn] = Rack.new(dn, ddesc, ddim, dd, db, dp)
			return true
		else
			t.errtok("rack field")
			t.next
		end
	end
end

def p_bars(t, defns, things)
	t.next
	nm = nil
	if t.op == TOK_LBRACE
		t.next
	else
		t.errtok("{")
		t.next
		return
	end
	if t.op == TOK_NAME
		nm = t.val
		t.next
	else
		t.errtok("name")
		t.next
		return
	end
	if (r = things[nm]) == nil
		sem_error("undefined instance #{nm}")
		return
	end
	while (b = p_bar(t, defns, things))
		r.addbar(b)
	end
	if t.op == TOK_RBRACE
		t.next
	else
		t.errtok("}")
		t.next
		return
	end
end

def p_inst(t, defns, things)
	t.next
	if t.op == TOK_LPAR
		t.next
	else
		t.errtok("(")
		t.next
		return nil
	end
	if t.op == TOK_NAME
		nd = t.val
		t.next
	else
		t.errtok("name")
		t.next
		return nil
	end
	if t.op == TOK_COMMA
		t.next
	else
		t.errtok(",")
		t.next
		return nil
	end
	if t.op == TOK_NAME
		nm = t.val
		t.next
	elsif t.op == TOK_STRING
		nm = t.val
		t.next
	else
		t.errtok("name")
		t.next
		return nil
	end
	if t.op == TOK_RPAR
		t.next
	else
		t.errtok(")")
		t.next
		return nil
	end
	things[nm] = Inst.new(nd, nm)
	return nm
end

def p_wire(t, defns, things)
	t.next
	if t.op == TOK_LPAR
		t.next
	else
		t.errtok("(")
		t.next
		return false
	end
	if t.op == TOK_STRING
		nm = t.val
		t.next
		if t.op == TOK_COMMA
			t.next
		else
			t.errtok(",")
			t.next
			return nil
		end
	else
		nm = nil
	end
	if (src = p_terminal(t, defns, things)) == nil
		return false
	end
	if t.op == TOK_COMMA
		t.next
	else
		t.errtok(",")
		t.next
		return false
	end
	if (sink = p_terminal(t, defns, things)) == nil
		return false
	end
	if t.op == TOK_COMMA
		t.next
	else
		t.errtok(",")
		t.next
		return false
	end
	if t.op == TOK_NAME
		wtype = t.val
		t.next
	else
		t.errtok("name")
		t.next
		return false
	end
	if t.op == TOK_RPAR
		t.next
	else
		t.errtok(")")
		t.next
		return false
	end
	if nm == nil
		nm = sprintf("%s_%s__%s_%s", src.inst, src.port, sink.inst, sink.port)
	end
	things[nm] = Wire.new(nm, src, sink, wtype)
	return true
end

def p_wiretype(t, defns, things)
	t.next
	if t.op == TOK_LBRACE
		t.next
	else
		t.errtok("{")
	end
	dn = nil
	ddesc = nil
	dvrml = nil
	da = nil
	while true
		case t.op
		when TOK_name
			t.next
			if t.op == TOK_STRING
				dn = t.val
				t.next
			else
				t.errtok("string")
			end
		when TOK_desc
			t.next
			if t.op == TOK_STRING
				ddesc = t.val
				t.next
			else
				t.errtok("string")
			end
		when TOK_alias
			t.next
			if t.op == TOK_STRING
				da = t.val
				t.next
			else
				t.errtok("string")
			end
		when TOK_vrml
			t.next
			if t.op == TOK_STRING
				dvrml = t.val
				t.next
			else
				t.errtok("string")
			end
		when TOK_RBRACE
			t.next
			defns[dn] = Wiretype.new(dn, ddesc, da, dvrml)
			return true
		else
			t.errtok("signal field")
		end
	end	
end

def p_attach(t, defns, things)
	t.next
	if t.op == TOK_LPAR
		t.next
	else
		t.errtok("(")
		t.next
		return false
	end
	if t.op == TOK_NAME
		origin = t.val
		t.next
	else
		t.errtok("name")
		t.next
		return false
	end
	if t.op == TOK_COMMA
		t.next
	else
		t.errtok(",")
		t.next
		return false
	end
	if (dim = p_dim(t, defns, things)) == nil
		return false
	end
	if t.op == TOK_COMMA
		t.next
	else
		t.errtok(",")
		t.next
		return false
	end
	if t.op == TOK_NAME
		who = t.val
		t.next
	elsif t.op == TOK_inst
		who = p_inst(t, defns, things)
	else
		t.errtok("name or inst")
		t.next
		return false
	end
	if t.op == TOK_RPAR
		t.next
	else
		t.errtok(")")
		t.next
		return false
	end
	if (i = things[who]) == nil
		t.error("unknown instance '#{who}")
		return false
	else
		i.attach(origin, dim)
	end
end

def p_terminal(t, defns, things)
	if t.op == TOK_NAME
		nm = t.val
		t.next
	else
		t.errtok("name")
		t.next
		return nil
	end
	if t.op == TOK_DOT
		t.next
	else
		t.errtok(",")
		t.next
		return nil
	end
	if t.op == TOK_NAME
		nmm = t.val
		t.next
	else
		t.errtok("name")
		t.next
		return nil
	end
	return Terminal.new(nm, nmm)
end

def p_anchor(t, defns, things)
	t.next
	if t.op == TOK_LPAR
		t.next
	else
		t.errtok("(")
		t.next
		return false
	end
	if t.op == TOK_NAME
		nm = t.val
		t.next
	else
		t.errtok("name")
		t.next
		return false
	end
	if t.op == TOK_COMMA
		t.next
	else
		t.errtok(",")
		t.next
		return false
	end
	if (dim = p_dim(t, defns, things)) == nil
		return false
	end
	if t.op == TOK_RPAR
		t.next
	else
		t.errtok(")")
		t.next
		return false
	end
	things[ANCHOR] = Anchor.new(nm, dim)
end

def p_vrml(t, defns, things)
	t.next
	if t.op == TOK_LBRACE
		t.next
	else
		t.errtok("{")
		t.next
		return false
	end
	s = ''
	while t.op == TOK_STRING
		s = s + t.val + "\n"
		t.next
	end
	if t.op == TOK_RBRACE
		t.next
	else
		t.errtok("}")
		t.next
		return false
	end
	things['vrml'] = s
	return true
end

def sem_error(s)
	puts "error: #{s}"
end

def place_pass(defns, things, pr)
	ntodo = 0
	npos = 0
	ndone = 0
	things.each{ |k,v|
		if v.class == Inst
			if v.pos == nil
				if v.origin.pos == nil
					if pr
						puts "	can't place #{k} because #{v.origin.name} is unplaced"
					end
					ntodo = ntodo+1
				else
					d = Dim.new(v.origin.pos.w.clone, v.origin.pos.h.clone, v.origin.pos.d.clone)
					d.add(v.dim)
					v.place(d)
					ndone = ndone+1
				end
			else
				npos = npos+1
			end
		end
	}
	return ntodo
end

def post_parse(defns, things)
	errs = false
	# first verify all necessary types have been defined
	things.each{ |k,v|
		if v.class == Inst
			if (d = defns[v.ndefn])
				v.set_defn(d)
			else
				sem_error("unknown defn '#{v.ndefn}' for '#{k}'")
				errs = true
			end
			if v.norigin
				if (d = things[v.norigin])
					v.set_origin(d)
				else
					sem_error("unknown attachment '#{v.norigin}' for '#{k}'")
					errs = true
				end
			end
		elsif v.class == Wire
			if !v.canon(defns)
				errs = true
			end
		end
	}
	if errs
		return false
	end
	# canonicalise all dimensions
	defns.each{ |k,v|
		if (v.class == Obj) || (v.class == Rack)
			v.canon
		end
	}
	# now verify all wires connect two known ports
	things.each{ |k,v|
		if v.class == Wire
			wtype = nil
			v.terms { |t|
				if (d = things[t.inst])
					if p = d.defn.port_by_name(t.port)
						d.connect(t.port)
						if wtype == nil
							wtype = p.signal
						else
							if wtype != p.signal
								sem_error("signal mismatch: #{wtype} and #{p.signal}")
								errs = true
							end
						end
					else
						sem_error("unknown port '#{t.port}' in '#{t.inst}'")
						errs = true
					end
				else
					sem_error("unknown terminal '#{t.inst}' for wire '#{k}'")
					errs = true
				end
				
			}
		end
	}
	if errs
		return false
	end
	# now anchor it all
	if (v = things[ANCHOR]) == nil
		sem_error("no anchor specified")
		return false
	end
	if (t = things[v.inst]) == nil
		sem_error("anchored instance #{t.inst} unknown")
		return false
	end
	t.place(v.dim)
	# now place everything else
	# place_pass returns number left to do
	last = -1
	while true
		i = place_pass(defns, things, false)
		if i == 0
			break		# done
		end
		if i == last
			sem_error("some things unattached; details follow")
			place_pass(defns, things, true)
			errs = true
		end
		last = i
	end
	# gather up global bars
	gbars = Array.new
	things.each{ |k,v|
		if (v.class == Inst) && (v.defn.class == Rack)
			bb = v.barset(nil, 'External')
			bb.each{ |b|
				if b.wtype == 'External'
					gbars << b
				end
			}
		end
	}
	things.each{ |k,v|
		if (v.class == Inst) && (v.defn.class == Rack)
			v.set_gbars(gbars)
		end
	}
	return !errs
end
