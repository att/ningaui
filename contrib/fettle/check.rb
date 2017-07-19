def check_phys(defns, things)
	errs = false
	# verify all boxes don't overlap (hope O(n^2) doesn't matter)
	things.each{ |k,v|
		if (v.class == Inst) && (v.defn.class == Obj)
			things.each{ |j,w|
				if (w.class == Inst) && (v != w) && (w.defn.class == Obj)
					if w.intersect(v)
						sem_error("#{v.name} intersects with #{w.name} || #{v.i1};#{v.i2}   #{w.i1};#{w.i2}")
						errs = true
					end
				end
			}
		end
	}
	if errs
		return false
	end
	return true
end

def check_wire(defns, things)
	errs = false
	things.each{ |k,v|
		if (v.class == Inst) && (v.defn.class == Obj)
			v.defn.ports.each{ |p|
				if !v.connected(p.name) && !p.sink
					sem_error("#{v.name}.#{p.name} not connected")
					errs = true
				end
			}
		end
	}
	if errs
		return false
	end
	return true
end
