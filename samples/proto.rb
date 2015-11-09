#!/usr/bin/env ruby
require "pp"
require "colored"
require "tty-screen"

Match = Struct.new(:line_index, :start_offset, :stop_offset, :match)

term_width, term_height = TTY::Screen.width, TTY::Screen.height
# term_width = 80

patt = %r{test/hash-fs.spec.js:26:8|sample.js:(\d+)}

T_ENTER_STANDOUT_MODE = "\033[7m"
T_EXIT_CA_MODE        = "\033[?1049l"
T_EXIT_STANDOUT_MODE  = "\033[0m"

T_CURSOR_UP = "\033[%dA"
T_COLUMN_ADDRESS = "\033[%dG"
T_ERASE_LINE = "\033[2K" # erase line
T_ERASE_DOWN = "\033[J" # erase from current line to bottom of screen

data = ''

line_start_offsets = []
text_line_offsets = []
matched_lines = []
selected_line = nil

f = File.open('testing-big.txt', 'r')
# f = File.open('errors.log', 'r')

while line = f.gets(term_width)
	abort "This shouldn't happen" if line.size > term_width

	if nl = line.index("\n")
		text_line_offsets << data.size+nl
	else
		# line_start_offsets << data.size
	end
	line_start_offsets << data.size

	if match = patt.match(line)
		matched_lines << Match.new(
			text_line_offsets.size,
			data.size + match.begin(0),
			data.size + match.end(0),
			match
		)
	end

	data << line
end

abort "No matches" if matched_lines.empty?

def start_index_for_selection_focus(start_offset, stop_offset, sel, line_start_offsets, height)
	if stop_offset.nil? or not (sel.start_offset >= start_offset and sel.stop_offset < stop_offset)
		selection_line_index = find_line_index(sel.start_offset, line_start_offsets)

		index = selection_line_index - height/2
		index = 0 if index < 0

		start_offset = line_start_offsets[index]

		stop_offset = find_end_offset(start_offset, line_start_offsets, height)
	end

	return start_offset, stop_offset
end

def find_line_index(offset, line_start_offsets)
	lines = line_start_offsets.size
	lines.times do |n|
		return n if line_start_offsets[n] == offset
		return n if line_start_offsets[n] < offset and offset < line_start_offsets[n+1]
	end
	nil
	# line_start_offsets.find { |i| i == start_offset }
end

# p find_line_index(64, line_start_offsets)

# abort "error" unless find_line_index(1, line_start_offsets) == 1
# abort "error" unless find_line_index(3, line_start_offsets) == 1
# abort "error" unless find_line_index(64, line_start_offsets) == 6
# abort "error" unless find_line_index(531, line_start_offsets) == 17
# exit

def find_end_offset(start_offset, line_start_offsets, height)
	index = find_line_index(start_offset, line_start_offsets)

	index += height
	index = line_start_offsets.size-1 if index >= line_start_offsets.size

	line_start_offsets[index]
end

def getc
	begin
		system("stty raw -echo")
		str = STDIN.getc
	ensure
		system("stty -raw echo")
	end
end

selected_index = 0
start_offset = 0
stop = nil

while true
	sel = matched_lines[selected_index]
	pp sel

	start_offset, stop = start_index_for_selection_focus(start_offset, stop, sel, line_start_offsets, term_height)
	p [start_offset, stop]
	exit

	# stop, lines = end_offset(start_offset, data, term_width, term_height)
	# stop = find_end_offset(start_offset, line_start_offsets, term_height)

	if sel.start_offset >= start_offset && sel.start_offset <= stop
		print data[start_offset...sel.start_offset]
		print T_ENTER_STANDOUT_MODE
		print data[sel.start_offset...sel.stop_offset]
		print T_EXIT_STANDOUT_MODE
		print data[sel.stop_offset...stop]
	else
		print data[start_offset...stop]
	end

	case getc
	when 'n'; selected_index += 1
	when 'p'; selected_index -= 1
	else
		exit
	end

	if selected_index == matched_lines.size
		selected_index = 0
	elsif selected_index == -1
		selected_index = matched_lines.size-1
	end

	printf T_CURSOR_UP, [line_start_offsets.size-1, term_height].min
	printf T_COLUMN_ADDRESS, 1
	print T_ERASE_DOWN
end
