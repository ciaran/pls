#include "patterns.h"
#include <string.h>
#include <stdio.h>

const char* default_patterns[] = {
	// Handle a possible colour sequence from clang output.
	"(?:\033\\[\\dm)?([\\/\\w\\-\\.]+\\.\\w+):(\\d+)(?::(\\d+))?",
	"file: ([\\/\\w.]+) line: (\\d+)",
	"in (.+?) on line (\\d+)",
	"([\\/\\w.]+)\\((\\d+),(\\d+)\\)",
	"File \"(.+?)\", line (\\d+)",
};

void
init_patterns (pattern_list_t* list)
{
	list->count = 0;
}

void
add_default_patterns (pattern_list_t* list)
{
	size_t n;
	for(n = 0; n < sizeof(default_patterns)/sizeof(*default_patterns); ++n)
		add_pattern(list, default_patterns[n]);
}

int
add_pattern (pattern_list_t* list, const char* str)
{
	const char *pcreErrorStr;
	int pcreErrorOffset;

	if (list->count+1 == MAX_PATTERNS)
		return 0;

	pattern_t* pattern = &list->patterns[list->count];

	pattern->str = calloc(strlen(str)+1, sizeof(*str));
	strcpy(pattern->str, str);

	pattern->compiled = pcre_compile(pattern->str, 0, &pcreErrorStr, &pcreErrorOffset, NULL);
	if (pcreErrorStr) {
		fprintf(stderr, "\033[1mError\033[0m: Could not compile '%s': %s\n", pattern->str, pcreErrorStr);
		exit(1);
	}

	pattern->extra = pcre_study(pattern->compiled, 0, &pcreErrorStr);
	if (pcreErrorStr) {
		fprintf(stderr, "\033[1mError\033[0m: Could not study '%s': %s\n", pattern->str, pcreErrorStr);
		exit(1);
	}

	++list->count;

	return 1;
}
