#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parse.h"
#include "editor.h"
#include "input.h"

#define assert_zu(a, b) if(a != b){ fprintf(stderr, "FAILURE (line %d): '%zu' != '%zu' (" #a " != " #b ")\n", __LINE__, (size_t)a, (size_t)b); exit(1); }
#define assert_str(a, b) if(0 != strcmp(a, b)){ fprintf(stderr, "FAILURE (line %d): '%s' != '%s'\n", __LINE__, a, b); exit(1); }

#define assert_field(field, value) assert(0 == strncmp(str+field.start, value, field.stop-field.start));

#define assert_cmd(format, result, field) \
	strcpy(cmd, format); \
	format_cmd(cmd, str, &field, NULL); \
	assert_str(cmd, result);

#define assert_skipped_sequence(seq) \
	assert_line_length(seq "foo\n", strlen(seq)+4, 4);

void
readfile (char* s, const char* filename)
{
	FILE* fd = fopen(filename, "r");
	assert(fd);
	fread(s, sizeof(char), BUFSIZ, fd);
	fclose(fd);
}

void
test_replace ()
{
	const char* s = "--foobar.js--13--14--";

	char buf[BUFSIZ];
	memset(buf, 0, BUFSIZ);
	strcpy(buf, "vim +%d");
	replace(buf, "%d", s+13, 2);
	assert_str(buf, "vim +13");

	strcpy(buf, "vim +%d:%d");
	replace(buf, "%d", s+13, 2);
	replace(buf, "%d", s+17, 2);
	assert_str(buf, "vim +13:14");

	strcpy(buf, "subl %s:%d");
	replace(buf, "%s", s+2, 9);
	replace(buf, "%d", s+13, 2);
	assert_str(buf, "subl foobar.js:13");
}

void
input_file (input_t* in, const char* filename, int width)
{
	FILE* fd = fopen(filename, "r");
	if(!fd)
	{
		fprintf(stderr, "ERROR: Missing fixture file %s\n", filename);
		exit(1);
	}
	input_init(in);
	while (input_read(in, fileno(fd), width, 0))
		;
	fclose(fd);
}

void
assert_line_length (char* s, int n, int width)
{
	const char* next_line = find_next_line(s, width);
	assert(next_line);
	assert_zu((next_line - s), n);
}

void
assert_consumed (char* s)
{
	assert(consume_escape_seq(s) == s+strlen(s));
}

void
test_lines ()
{
	input_t in;

	assert_consumed("\033[1m");
	assert_consumed("\033[1;2m");

	assert_line_length("foo\nbar",  4, 80);
	assert_line_length("\nbar",     1, 80);
	assert_line_length("foobarbaz", 4, 4);

	assert_skipped_sequence("\033[1m");

	input_file(&in, "samples/errors.log", 80);
	assert_zu(in.nlines, 10);
	assert(in.line_offsets[3] == 22);
	input_free(&in);

	input_file(&in, "samples/errors.log", 10);
	assert_zu(in.nlines, 21);
	assert(in.line_offsets[3] == 21);
	input_free(&in);

	input_file(&in, "samples/testing-big.txt", 80);
	assert(find_line_index(&in, 1)  == 1);
	assert(find_line_index(&in, 3)  == 1);
	assert(find_line_index(&in, 64) == 6);
	assert(find_line_index(&in, 531) == 17);
	input_free(&in);
}

void
test_parse ()
{
	char cmd[BUFSIZ];
	char str[BUFSIZ];

	pattern_list_t patterns;
	init_patterns(&patterns);
	add_default_patterns(&patterns);

	readfile(str, "samples/simple.txt");

	assert(study(&patterns, str, strlen(str), 0));

	assert(field_count == 5);

	assert_field(field_offsets[0].path, "foobar.js");
	assert_field(field_offsets[0].line, "16");
	assert(field_offsets[0].column.start == 0);

	assert_cmd("vim +'call cursor(%d, %d)'", "vim +'call cursor(16, 0)' foobar.js", field_offsets[0]);

	assert_field(field_offsets[2].path, "foobar.js");
	assert_field(field_offsets[2].line, "14");
	assert_field(field_offsets[2].column, "6");

	assert_cmd("subl %s:%d", "subl foobar.js:14", field_offsets[2]);
	assert_cmd("vim +%d",    "vim +14 foobar.js", field_offsets[2]);
	assert_cmd("vim +'call cursor(%d, %d)'", "vim +'call cursor(14, 6)' foobar.js", field_offsets[2]);
}

void
test_editor ()
{
	char* cmd;

	assert(is_named_executable("foo", "foo"));
	assert(is_named_executable("foo -test", "foo"));
	assert(is_named_executable("/usr/bin/foo", "foo"));
	assert(is_named_executable("/usr/bin/foo -test", "foo"));

	unsetenv("EDITOR");
	unsetenv("PLS");

	cmd = editor_command();
	assert_str(cmd, EDITOR_COMMAND);

	setenv("EDITOR", "atom", 1);
	cmd = editor_command();
	assert_str(cmd, "atom %s:%d:%d");

	setenv("EDITOR", "/usr/bin/subl -w", 1);
	cmd = editor_command();
	assert_str(cmd, "/usr/bin/subl -w %s:%d:%d");

	setenv("PLS", "Foo bar", 1);
	cmd = editor_command();
	assert_str(cmd, "Foo bar");
}

int main(int argc, char const *argv[])
{
	(void)argc; (void)argv;

	test_editor();

	test_lines();

	test_replace();

	test_parse();

	return 0;
}
