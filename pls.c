#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <termios.h>
#include <signal.h>
#include <wordexp.h>
#include "parse.h"
#include "input.h"
#include "editor.h"

static input_t in;

/* Terminal capabilities */
#define T_ERASE_DOWN          "\033[J"
#define T_COLUMN_ADDRESS      "\033[%dG"
#define T_CURSOR_INVISIBLE    "\033[?25l"
#define T_CURSOR_UP           "\033[%dA"
#define T_CURSOR_VISIBLE      "\033[?25h"
#define T_ENTER_CA_MODE       "\033[?1049h"
#define T_ENTER_STANDOUT_MODE "\033[7m"
#define T_EXIT_CA_MODE        "\033[?1049l"
#define T_RESET_SGR           "\033[0m"

#define ESCAPE      27
#define UP_ARROW    65
#define DOWN_ARROW  66
#define RIGHT_ARROW 67
#define LEFT_ARROW  68

#define CONTROL(c) (c ^ 0x40)
#define MAX(x, y) (x > y ? x : y)
#define MIN(x, y) (x < y ? x : y)

static const char **utility;

static struct {
	int in;
	int out;
	int ca;              /* use alternate screen */
	unsigned int height;
	unsigned int width;
	struct termios attr;
} tty;

static void args(int, const char **);
static void editor(void);

static void tend(void);
static void tdraw(const char *s, size_t start, size_t stop);
static void tmain(void);
static void tprintf(const char *, int);
static void tputs(const char *);
static void tsetup(void);
static void twrite(const char *, size_t);

static ssize_t xwrite(int, const char *, size_t);

static int selection_index = -1;

static struct {
	int initial_last; // start with last field selected instead of first
	int always_select;
	int only_existing;

	char* paths[100];
	int path_count;
} options;

void
args(int argc, const char **argv)
{
	int c, i;

	while ((c = getopt(argc, (char * const *) argv, "lavehp:")) != -1) {
		switch (c) {
		case 'v':
			puts("pls " VERSION);
			exit(0);
		case 'l':
			options.initial_last = 1;
			break;
		case 'a':
			options.always_select = 1;
			break;
		case 'e':
			options.only_existing = 1;
			break;
		case 'p':
			options.paths[options.path_count] = optarg;
			++options.path_count;
			break;
		case 'h':
		default:
			puts("usage: pls [-lae] [-p path] utility\n");
			if (c == 'h') {
				puts("Arguments:"
				"\n  -e          Only select existing filenames"
				"\n  -l          Set initial selection to the last path"
				"\n  -p          Add path to the list of directories searched for selected files"
				"\n  -a          Show selection interface even if utility exits with 0 status"
				);
			}
			exit(1);
		}
	}

	// Remaining arguments are invoked as the utility command
	utility = calloc(argc - optind + 2, sizeof(const char *));
	if (!utility)
		perror("calloc");
	for (i = optind; i < argc; i++)
		utility[i - optind] = argv[i];
}

void
editor(void)
{
	char* cmd = editor_command();
	const char* path = NULL;
	const struct field_t* field = &field_offsets[selection_index];

	if (field->path_index > 0)
		path = options.paths[field->path_index-1];

	format_cmd(cmd, in.v, field, path);

	// vim need stdin to be a tty
	(void)freopen("/dev/tty", "r", stdin);

	system(cmd);
}

ssize_t
xwrite(int fd, const char *s, size_t nmemb)
{
	ssize_t r;
	size_t n = nmemb;

	do {
		r = write(fd, s, n);
		if (r < 0)
			return r;
		n -= r;
		s += r;
	} while (n);

	return nmemb;
}

void
tdraw(const char *s, size_t start, size_t stop)
{
	static size_t output_start = 0;
	static size_t output_stop = 0;

	// Only move the visible region if necessary
	if(!(start >= output_start && stop < output_stop))
	{
		size_t index = find_line_index(&in, start);
		if(index < tty.height/2)
			index = 0;
		else
			index -= tty.height/2;

		output_start = in.line_offsets[index];

		if(index + tty.height >= in.nlines)
			output_stop = in.nmemb;
		else
			output_stop = in.line_offsets[index+tty.height-1];
	}

	twrite(s+output_start, start-output_start);
	tputs(T_ENTER_STANDOUT_MODE);
	twrite(s + start, stop - start);
	tputs(T_RESET_SGR);
	twrite(s + stop, output_stop - stop);
}

void
tprintf(const char *format, int x)
{
	char s[32];
	int n;

	n = snprintf(s, sizeof(s), format, x);

	twrite(s, n);
}

void
tputs(const char *s)
{
	size_t n = strlen(s);

	twrite(s, n);
}

void
twrite(const char *s, size_t nmemb)
{
	if (xwrite(tty.out, s, nmemb) < 0)
		perror("write");
}

void
tinfo(void)
{
	struct winsize ws;

	tty.in = open("/dev/tty", O_RDONLY);
	if (!tty.in)
		perror("open");
	if (ioctl(tty.in, TIOCGWINSZ, &ws) < 0) {
		perror("ioctl");
		tty.height = 24;
		tty.width = 80;
	} else {
		tty.height = ws.ws_row;
		tty.width = ws.ws_col;
	}
}

void
tsetup(void)
{
	struct termios attr;

	tcgetattr(tty.in, &tty.attr);
	memcpy(&attr, &tty.attr, sizeof(struct termios));
	attr.c_lflag &= ~(ICANON|ECHO|ISIG);
	tcsetattr(tty.in, TCSANOW, &attr);

	tty.out = open("/dev/tty", O_WRONLY);
	if (!tty.out)
		perror("open");

	if (tty.ca)
		tputs(T_ENTER_CA_MODE);
	tputs(T_CURSOR_INVISIBLE);
}

void
tend(void)
{
	tputs(T_RESET_SGR);
	tputs(T_CURSOR_VISIBLE);
	tcsetattr(tty.in, TCSANOW, &tty.attr);

	close(tty.in);
	close(tty.out);
}

enum
{
	none,
	edit,
	quit,
	prev, next,
	first, last,
}
read_command ()
{
	char c[3];

	if (read(tty.in, &c, 3) < 0)
		perror("read");

	if (c[0] == ESCAPE) {
		if (c[1] != '[')
			return none;

		switch (c[2]) {
		case 'Z': /* ESC[Z = shift-tab */
		case UP_ARROW:
		case LEFT_ARROW:
			return prev;
		case DOWN_ARROW:
		case RIGHT_ARROW:
			return next;
		default:
			return none;
		}
	}

	switch (c[0]) {
	case '\n':
		return edit;
	case CONTROL('C'):
	case CONTROL('D'):
	case 'q':
		return quit;
	case '\t':
	case CONTROL('N'):
	case 'j':
		return next;
	case CONTROL('P'):
	case 'k':
		return prev;
	case CONTROL('A'):
		return first;
	case CONTROL('E'):
		return last;
	}

	return none;
}

void
tmain(void)
{
	size_t start, stop;
	size_t field_index;

	start = stop = 0;

	if(options.initial_last)
		field_index = field_count-1;
	else
		field_index = 0;

	for (;;) {
		start = field_offsets[field_index].match.start;
		stop = field_offsets[field_index].match.stop;

		tdraw(in.v, start, stop);

		switch (read_command()) {
		case edit:
			selection_index = field_index;
			return;
		case quit:
			return;
		case first:
			field_index = 0;
			break;
		case next:
			if (++field_index == field_count)
				field_index = 0;
			break;
		case last:
			field_index = field_count-1;
			break;
		case prev:
			if(field_index == 0)
				field_index = field_count-1;
			else
				--field_index;
			break;
		case none:
			continue;
		}

		if (in.nlines)
			tprintf(T_CURSOR_UP, in.nlines);
		tprintf(T_COLUMN_ADDRESS, 1);
		tputs(T_ERASE_DOWN);
		tputs(T_RESET_SGR);
	}
}

#define PIPES 2

int
run_utility (void)
{
	pid_t pid;
	int status;
	int filedes[PIPES][2];
	int n;
	int max_fd;
	fd_set out_fds;

	if(pipe(filedes[0]) == -1 || pipe(filedes[1]) == -1) {
		perror("pipe");
		exit(1);
	}

	pid = fork();
	if(pid == -1) {
		perror("fork");
		exit(1);
	}

	if (pid == 0) {
		char* const* argv = (char * const*)utility;

		dup2(filedes[0][1], STDOUT_FILENO);
		dup2(filedes[1][1], STDERR_FILENO);

		close(filedes[1][0]);
		close(filedes[1][1]);
		close(filedes[0][0]);
		close(filedes[0][1]);

		execvp(argv[0], (char * const*)argv);
		perror("execv");
		exit(1);
	}

	close(filedes[0][1]);
	close(filedes[1][1]);

	while(1)
	{
		FD_ZERO(&out_fds);
		max_fd = 0;

		for(n = 0; n < PIPES; ++n) {
			if(filedes[n][0] != 0) {
				FD_SET(filedes[n][0], &out_fds);
				if(filedes[n][0] > max_fd)
					max_fd = filedes[n][0];
			}
		}
		if(max_fd == 0)
			break;

		int count = select(FD_SETSIZE, &out_fds, NULL, NULL, NULL);
		if(count == -1) {
			perror("select");
			exit(1);
		}
		if (count > 0)
		{
			for(n = 0; n < PIPES; ++n)
			{
				if(FD_ISSET(filedes[n][0], &out_fds))
				{
					if(0 == input_read(&in, filedes[n][0], tty.width, 1)) {
						close(filedes[n][0]);
						filedes[n][0] = 0;
					}
				}
			}
		}
	}

	waitpid(pid, &status, 0);

	// TODO: there is more to check here than > 0,
	// `man 3 wait` for details.

	return status;
}

void
readrc(pattern_list_t* list)
{
	char line[255];
	wordexp_t exp_result;
	wordexp("~/.plsrc", &exp_result, 0);

	const char* path = exp_result.we_wordv[0];

	if(0 != access(path, F_OK))
		return;

	FILE* fd = fopen(path, "r");

	while(fgets(line, 255, fd))
	{
		size_t len = strlen(line);
		if(len <= 2) // Lazy blank line test
			continue;

		if(line[0] == '#')
			continue;

		if(line[len-1] == '\n')
			line[len-1] = '\0';

		add_pattern(list, line);
	}

	fclose(fd);
}

int
exists (const char* path, size_t len)
{
	char buf[PATH_MAX];
	strncpy(buf, path, len);
	buf[len] = '\0';

	return 0 == access(buf, F_OK);
}

int valid_field (const char* s, struct field_t* field)
{
	if (!options.only_existing)
		return 1;

	const char* path = s+field->path.start;
	size_t length = field->path.stop-field->path.start;
	char buf[PATH_MAX] = {0};
	int n;

	strncpy(buf, path, length);

	if(0 == access(buf, F_OK))
		return 1;

	for (n = 0; n < options.path_count; ++n)
	{
		strcpy(buf, options.paths[n]);
		strcat(buf, "/");
		strncat(buf, path, length);

		if(0 == access(buf, F_OK)) {
			field->path_index = n+1;
			return 1;
		}
	}

	return 0;
}

int
main(int argc, const char *argv[])
{
	if (!isatty(fileno(stdout))) {
		fprintf(stderr, "\033[1mError\033[0m: output is not a terminal\n");
		exit(1);
	}

	pattern_list_t patterns;
	patterns.count = 0;
	init_patterns(&patterns);
	add_default_patterns(&patterns);

	readrc(&patterns);

	args(argc, argv);

	if (patterns.count == 0) {
		fprintf(stderr, "\033[1mError\033[0m: no patterns loaded!\n");
		exit(1);
	}

	tinfo();

	input_init(&in);
	if(argc - optind > 0) {
		if (run_utility() == 0 && !options.always_select)
			exit(0);
	} else {
		while(input_read(&in, STDIN_FILENO, tty.width, 1))
			;
	}

	if(in.nmemb == 0)
		exit(0);

	study(&patterns, in.v, in.nmemb, &valid_field);

	if (field_count == 0)
		return 0;

	tsetup();

	// Since we echo the input as we receive it,
	// we need to rewind back up to the start.
	if (in.nlines)
		tprintf(T_CURSOR_UP, MIN(in.nlines, tty.height));
	tprintf(T_COLUMN_ADDRESS, 1);
	tputs(T_ERASE_DOWN);

	tmain();
	tend();

	if (selection_index >= 0)
		editor();

	return 0;
}
