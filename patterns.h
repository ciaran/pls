#ifndef PATTERNS_H
#define PATTERNS_H

#include <pcre.h>

#define MAX_PATTERNS 100

typedef struct {
	char* str;
	pcre* compiled;
	pcre_extra* extra;
} pattern_t;

typedef struct {
	pattern_t patterns[MAX_PATTERNS];
	int count;
} pattern_list_t;

void init_patterns (pattern_list_t* list);

void add_default_patterns (pattern_list_t* list);

int add_pattern (pattern_list_t* list, const char* str);

#endif
