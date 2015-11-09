#ifndef INPUT_H
#define INPUT_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

typedef struct {
	size_t size;
	size_t nmemb;

	size_t nlines; // number of display lines

	size_t *line_offsets;
	size_t line_offset_size; // current size of line_offsets array

	char *v;
} input_t;

void input_init (input_t* input);
void input_free (input_t* input);

size_t input_read (input_t* input, int fd, int width, int echo);

size_t find_line_index (input_t* input, size_t offset);

size_t find_end_offset (input_t* input, size_t stop_offset, size_t height);

// (private)
char* find_next_line (char* s, int width);
char* consume_escape_seq (char* c);

#endif
