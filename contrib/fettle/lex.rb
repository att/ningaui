#!/usr/bin/ruby

class Token
	def initialize
		@saved = false
		@eof = false
		@line = 1
		@nerrors = 0
		@namechar = Array.new
		truthify(@namechar, 48, 57)	# 0-9
		truthify(@namechar, 97, 122)	# a-z
		truthify(@namechar, 65, 90)	# A-Z
		@namechar[95] = true		# _
		@isdigit = Array.new
		truthify(@isdigit, 48, 57)	# 0-9
		@namemap = {
			'object' => TOK_object,
			'rack' => TOK_rack,
			'inst' => TOK_inst,
			'attach' => TOK_attach,
			'wire' => TOK_wire,
			'name' => TOK_name,
			'desc' => TOK_desc,
			'dim' => TOK_dim,
			'ports' => TOK_ports,
			'anchor' => TOK_anchor,
			'doors' => TOK_doors,
			'wiretype' => TOK_wiretype,
			'vrml' => TOK_vrml,
			'alias' => TOK_alias,
			'bars' => TOK_bars,
			'sink' => TOK_sink
		}
		@unitmap = Array.new
		@unitmap[85] = UNIT_u
		@unitmap[117] = UNIT_u
		@unitmap[82] = UNIT_rack
		@unitmap[114] = UNIT_rack
		@unitmap[73] = UNIT_inch
		@unitmap[105] = UNIT_inch
	end

	def truthify(a, b, e)
		for i in b .. e
			a[i] = true
		end
	end

	def undo
		@saved = true
	end

	def op
		@op
	end

	def val
		@val
	end

	def sign
		@sign
	end

	def unit
		@unit
	end

	def line
		@line
	end

	def nerrors
		@nerrors
	end

	def print
		puts "#{str()}"
	end

	def str
		s = "#{@op}"
		case @op
		when TOK_STRING
			s = s + " '#{@val}'"
		when TOK_NAME
			s = s + " '#{@val}'"
		when TOK_NUMBER
			s = s + " v=#{@val} s=#{@sign} u=#{@unit}"
		end
		"token: #{s} line=#{@line} eof=#{@eof}"
	end

	def error(s)
		puts "#{@line}: error: " + s
		@nerrors = @nerrors+1
		sleep(1)
	end

	def errtok(s)
		error("expected #{s}, got #{str()}")
	end

	def whitespace
		while (@ch = STDIN.getc) != nil
			case @ch
			when 10	# newline
				@line = @line+1
			when 32,9	# space or tab
				@ch = 0
			when 35	# comment
				while (@ch = STDIN.getc) != 10
					if @ch == nil
						@eof = true
						return
					end
				end
			else
				return
			end
		end
		@eof = true
	end

	def eatname
		s = '' << @ch
		while @namechar[(@ch = STDIN.getc)]
			s.concat(@ch)
		end
		@val = s
		@eof = @ch == nil
		if !@eof
			STDIN.ungetc(@ch)
		end
		if (@op = @namemap[@val]) == nil
			@op = TOK_NAME
		end
	end

	def eatstring
		sl = @line
		s = ''
		while true
			@ch = STDIN.getc
			if @ch == nil
				error("unterminated string started on line #{sl}")
				break
			end
			if @ch == 34
				break
			elsif @ch == 92		# \
				@ch = STDIN.getc
				if @ch == nil
					error("unterminated string started on line #{sl}")
					break
				end
			end
			s.concat(@ch)
		end
		@val = s
puts ">>#{s}<"
		@eof = @ch == nil
	end

	def eatnumber
		@op = TOK_NUMBER
		@unit = UNIT_none
		@val = 0
		case @ch
		when 43
			@sign = 1
			@ch = STDIN.getc
		when 45
			@sign = -1
			@ch = STDIN.getc
		else
			@sign = 0
		end
		if !@isdigit[@ch]
			error("sign not followed by a number")
			return
		end
		scale = 1
		decimal = false
		while @isdigit[@ch] || (@ch == 46)
			if @ch == 46
				scale = 1
				decimal = true
			else
				@val = 10*@val + @ch - 48
				scale = scale*10
			end
			@ch = STDIN.getc
		end
		if decimal
			@val = Float.induced_from(@val) / scale
		end
		@unit = @unitmap[@ch]
		if @unit == nil
			@unit = UNIT_none
		else
			@ch = STDIN.getc
		end 
		@eof = @ch == nil
		if !@eof
			STDIN.ungetc(@ch)
		end
	end

	def next
		if @saved
			@saved = false
			return
		end
		if @eof
			return
		end
		whitespace
		case @ch
		when nil
			@op = TOK_EOF
		when 34	# "
			@op = TOK_STRING
			eatstring
		when 40
			@op = TOK_LPAR
		when 41
			@op = TOK_RPAR
		when 44
			@op = TOK_COMMA
		when 46
			@op = TOK_DOT
		when 43, 45		# plus minus
			eatnumber
		when 91
			@op = TOK_LBRACK
		when 93
			@op = TOK_RBRACK
		when 123
			@op = TOK_LBRACE
		when 125
			@op = TOK_RBRACE
		else
			if @isdigit[@ch]
				eatnumber
			elsif @namechar[@ch]
				eatname
			else
				error("unexpected character #{@ch}=".concat(@ch))
				@op = TOK_LEXERROR
			end
		end
	end
end
