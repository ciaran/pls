#include <ctype.h>
#include "input.h"

// ESC \d+ [;\d+ [; ...]] m
char*
consume_escape_seq (char* c)
{
	if(*c != '\033' || c[1] != '[')
		return c;

	++c;

	do
	{
		++c;

		while(isdigit(*c))
			++c;
	}
	while(*c == ';');

	++c;

	return c;
}

char*
find_next_line (char* line_start, int width)
{
	int display_width = 0;
	char* c = line_start;

	while(*c != '\0' && display_width < width)
	{
		if(*c == '\n')
			return c+1;

		if(*c == '\033')
		{
			c = consume_escape_seq(c);
		}
		else
		{
			++c;
			++display_width;
		}
	}

	if(display_width == width)
		return line_start+width;

	return NULL;
}

size_t
input_read (input_t* input, int fd, int width, int echo)
{
	if (input->size == 0) {
		fprintf(stderr, "ERROR: read before init\n");
		exit(1);
	}

	int n = read(fd, input->v + input->nmemb, input->size - input->nmemb-1);
	if (n < 0) {
		perror("read");
		return 0;
	}

	if (!n)
		return 0;

	input->v[input->nmemb + n] = '\0';

	if(echo)
		printf("%s", input->v + input->nmemb);

	char* line_start = input->v + input->line_offsets[input->nlines];

	while((line_start = find_next_line(line_start, width)))
	{
		++input->nlines;

		input->line_offsets[input->nlines] = line_start - input->v;

		if (input->line_offset_size < input->nlines + BUFSIZ) {
			input->line_offset_size += BUFSIZ;
			input->line_offsets = realloc(input->line_offsets, input->line_offset_size*sizeof(*input->line_offsets));
		}
	}

	input->nmemb += n;

	if (input->size < input->nmemb + BUFSIZ) {
		input->size *= 2;
		input->v = realloc(input->v, input->size);
		if (!input->v)
			perror("realloc");
	}
	return 1;
}

size_t
find_line_index (input_t* input, size_t offset)
{
	size_t i;
	for (i = 0; i < input->nlines; ++i)
	{
		if(input->line_offsets[i] == offset)
			return i;
		if(i == input->nlines-1)
			return i;
		if(input->line_offsets[i] < offset && offset < input->line_offsets[i+1])
			return i;
	}
	fprintf(stderr, "ERROR: Couldn't find line index: %zu\n", offset);
	exit(1);
}

size_t
find_end_offset (input_t* input, size_t stop_offset, size_t height)
{
	size_t index = find_line_index(input, stop_offset);

	index += height;
	if(index >= input->nlines)
		index = input->nlines-1;

	return input->line_offsets[index];
}

void
input_init (input_t* input)
{
	input->size = BUFSIZ;
	input->nmemb = 0;
	input->v = malloc(input->size);
	if (!input->v) {
		perror("malloc");
		exit(1);
	}

	input->nlines = 0;
	input->line_offset_size = BUFSIZ;
	input->line_offsets = calloc(input->line_offset_size, sizeof(*input->line_offsets));
	input->line_offsets[0] = 0;
}

void
input_free (input_t* input)
{
	if(input->size > 0)
		free(input->v);
	if(input->line_offset_size > 0)
		free(input->line_offsets);
}
