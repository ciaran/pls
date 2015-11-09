#include <pcre.h>
#include <assert.h>
#include <unistd.h>
#include <limits.h>
#include "patterns.h"

#define MAX_FIELDS 100

static size_t field_count = 0;
static struct field_t {
	struct {
		size_t start;
		size_t stop;
	} match, path, line, column;
	int path_index;
} field_offsets[MAX_FIELDS];

typedef int (valid_field_t) (const char* s, struct field_t* field);

int
match_line (const char* line, int length, size_t offset, pattern_list_t* list, struct field_t* field)
{
	int pcreExecRet;
	int subStrVec[30];
	int i;

	for (i = 0; i < list->count; ++i) {
		pattern_t* pattern = &list->patterns[i];

		pcreExecRet = pcre_exec(pattern->compiled, pattern->extra, line, length, 0, 0, subStrVec, sizeof(subStrVec) / sizeof(int));

		if (pcreExecRet > 0) {
			if (pcreExecRet == 1) {
				fprintf(stderr, "Warning: no captures in match\n");
				continue;
			}

			field->match.start = offset+subStrVec[0];
			field->match.stop  = offset+subStrVec[1];

			field->path.start = offset+subStrVec[2];
			field->path.stop  = offset+subStrVec[3];

			if (pcreExecRet > 2) {
				field->line.start = offset+subStrVec[4];
				field->line.stop  = offset+subStrVec[5];
			}
			if (pcreExecRet > 3) {
				field->column.start = offset+subStrVec[6];
				field->column.stop  = offset+subStrVec[7];
			}

			return 1;
		}
	}

	return 0;
}

int study (pattern_list_t* patterns, const char *s, size_t length, valid_field_t* valid_field)
{
	const char* line;
	char* newline;
	size_t offset;
	int lineLength;
	struct field_t field;

	assert(length > 0);

	field_count = 0;

	for (offset = 0; offset < length; ) {
		line = s+offset;

		newline = strchr(line, '\n');

		if (newline != NULL) {
			lineLength = newline - line;
		} else {
			lineLength = length-offset;
		}

		memset(&field, 0, sizeof(field));
		if (match_line(line, lineLength, offset, patterns, &field)) {
			if (!valid_field || valid_field(s, &field)) {
				field_offsets[field_count] = field;
				field_count++;

				if (field_count == MAX_FIELDS)
					break;
			}
		}

		offset += lineLength+1;
	}

	return 1;
}

int
replace (char* out, const char* placeholder, const char* in, int length)
{
	char buf[200] = {0};
	char* pos = strstr(out, placeholder);

	if (pos == NULL)
		return 0;

	strncat(buf, out, pos-out);
	strncat(buf, in, length);
	strcat(buf, pos+strlen(placeholder));
	strcpy(out, buf);

	return 1;
}

int
format_cmd (char* out, const char* s, struct field_t const* field, const char* path)
{
	if(field->line.start > 0)
			replace(out, "%d", s+field->line.start, field->line.stop-field->line.start);
	else	replace(out, "%d", "0", 1);

	if(field->column.start > 0)
			replace(out, "%d", s+field->column.start, field->column.stop-field->column.start);
	else	replace(out, "%d", "0", 1);

	char buf[PATH_MAX] = {0};
	if (path) {
		strcat(buf, path);
		strcat(buf, "/");
	}
	strncat(buf, s+field->path.start, field->path.stop-field->path.start);

	if (!replace(out, "%s", buf, PATH_MAX)) {
		// If there’s no explicit placeholder for the file path,
		// we append it to the end of the command.
		size_t len = strlen(out);
		if(out[len-1] != ' ')
			strcat(out, " ");
		strcat(out, buf);
	}

	return 1;
}
