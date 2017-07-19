Bar_wid = 0.5

def vrml_label(fp, i)
	x, y, z = i.i2.x-i.i1.x, i.i2.y-i.i1.y, i.i2.z-i.i1.z
	fp.printf("Transform { # %s\n", i.name)
	fp.printf("\ttranslation %.2f %.2f %.2f\n", i.i1.x+0.5, i.i1.y+0.2, -i.i1.z+0.01)
	fp.printf("\tchildren\n")
	fp.printf("\tShape {\n")
	fp.printf("\t\tappearance USE %s\n", 'Text_app')
	fp.printf("\t\tgeometry Text {\n")
	fp.printf("\t\t\tstring [\"%s\"]\n", i.name)
#	fp.printf("\t\t\tfontStyle Label_font {\n")
#	fp.printf("\t\t\t\tsize %.2f\n", 1)
#	fp.printf("\t\t\t\tjustify \"LEFT\"\n")
#	fp.printf("\t\t\t}\n")
	fp.printf("\t\t}\n")
	fp.printf("\t}\n")
	fp.printf("}\n")
end

def vrml_box(fp, i)
	x, y, z = i.i2.x-i.i1.x, i.i2.y-i.i1.y, i.i2.z-i.i1.z
	fp.printf("Transform { # %s\n", i.name)
	fp.printf("\ttranslation %.2f %.2f %.2f\n", i.i1.x+x/2.0, i.i1.y+y/2.0, -i.i1.z-z/2.0)
	fp.printf("\tchildren\n")
	fp.printf("\tShape {\n")
	if i.defn.vrml
		a = i.defn.vrml
	else
		a = "Box_default"
	end
	fp.printf("\t\tappearance USE %s\n", a)
	fp.printf("\t\tgeometry Box {\n")
	fp.printf("\t\t\tsize %.2f %.2f %.2f\n", x, y, z)
	fp.printf("\t\t}\n")
	fp.printf("\t}\n")
	fp.printf("}\n")

	vrml_label(fp, i)
end

def vrml_rack(fp, i)
	i.defn.each_door{ |d|
		fp.printf("Transform { # %s door\n", i.name)
		c = i.i1.add(d.origin.convert3)
		s = d.dim.convert3
		fp.printf("\ttranslation %.2f %.2f %.2f\n", c.x+s.x/2.0, c.y+s.y/2.0, -c.z-s.z/2.0)
		fp.printf("\tchildren\n")
		fp.printf("\tShape {\n")
		a = d.vrml
		if a == nil
			a = "Rack_door"
		end
		fp.printf("\t\tappearance USE %s\n", a)
		fp.printf("\t\tgeometry Box {\n")
		fp.printf("\t\t\tsize %.2f %.2f %.2f\n", s.x, s.y, s.z)
		fp.printf("\t\t}\n")
		fp.printf("\t}\n")
		fp.printf("}\n")
	}
	i.barset(nil, nil).each{ |b|
		fp.printf("Transform { # %s bar\n", i.name)
		c = b.origin
		s = b.dim
		if s.xzero && s.yzero			# z-axis bar
			s = Coord3.new(Bar_wid, Bar_wid, s.z)
			c = c.add(Coord3.new(0, 0, s.z/2.0))
		elsif s.xzero && s.zzero		# y-axis bar
			s = Coord3.new(Bar_wid, s.y, Bar_wid)
			c = c.add(Coord3.new(0, s.y/2.0, 0))
		elsif s.yzero && s.zzero		# x-axis bar
			s = Coord3.new(s.x, Bar_wid, Bar_wid)
			c = c.add(Coord3.new(s.x/2.0, 0, 0))
		else
			# puts "!!!!! skew bar #{bar}"
		end
		fp.printf("\ttranslation %.2f %.2f %.2f\n", c.x, c.y, -c.z)
		fp.printf("\tchildren\n")
		fp.printf("\tShape {\n")
		a = "Rack_bar"
		fp.printf("\t\tappearance USE %s\n", a)
		fp.printf("\t\tgeometry Box {\n")
		fp.printf("\t\t\tsize %.2f %.2f %.2f\n", s.x, s.y, s.z)
		fp.printf("\t\t}\n")
		fp.printf("\t}\n")
		fp.printf("}\n")
	}
	vrml_label(fp, i)
end

def vrml_wire(fp, w, cables)
	a = cables.wire(w.name)
	if a == nil
		return
	end
	fp.printf("Transform { # %s\n", w.name)
	fp.printf("\ttranslation %.2f %.2f %.2f\n", 0, 0, 0)
	fp.printf("\tchildren\n")
	fp.printf("\tShape {\n")
	ap = w.vrml
	if ap == nil
		ap = 'Wire_default'
	end
	fp.printf("\t\tappearance USE %s \n", ap)
	fp.printf("\t\tgeometry Extrusion {\n")
	fp.printf("\t\t\tcrossSection [.1 .1, .1 -.1, -.1 -.1, -.1 .1, .1 .1]\n")
	fp.printf("\t\t\tspine")
	sep = '['
	a.each{ |p|
		fp.printf("%s %.2f %.2f %.2f", sep, p.p.x, p.p.y, -p.p.z)
		sep = ','
	}
	fp.printf("]\n")
	fp.printf("\t\t}\n")
	fp.printf("\t}\n")
	fp.printf("}\n")
end

def vrml_pre(fp, s)
	fp.printf("#VRML V2.0 utf8\n\n")
	if s
		fp.printf("%s\n", s)
	end
end

def vrml_post(fp)
	fp.printf("DEF view1 Viewpoint {\n")
	fp.printf("\tposition 20 40 80\n")
	fp.printf("\torientation 0 0 -1 0\n")
	fp.printf("\tfieldOfView 1\n")
	fp.printf("\tdescription \"front view\"\n")
	fp.printf("}\n")
	fp.printf("\nDEF view2 Viewpoint {\n")
	fp.printf("\tposition 20 40 80\n")
	fp.printf("\torientation 0 0 -1 0\n")
	fp.printf("\tfieldOfView 1\n")
	fp.printf("\tdescription \"view2\"\n")
	fp.printf("}\n")
	fp.printf("Background { skyColor [.3, .6, .3] }\n")
end

def vrml_output(defns, things, cables)
	fp = File.new("goo.wrl", "w")
	vrml_pre(fp, things['vrml'])
	things.each{ |k,v|
		if v.class == Inst
			if v.defn.class == Rack
				vrml_rack(fp, v)
			else
				vrml_box(fp, v)
			end
		elsif v.class == Wire
			vrml_wire(fp, v, cables)
		end
	}
	vrml_post(fp)
end
