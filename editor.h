#ifndef EDITOR_H
#define EDITOR_H

#include <string.h>
#include <libgen.h>

const char* EDITOR_COMMAND = "vim +'call cursor(%d, %d)'";

// is_named_executable("/foo/bar/baz -n1", "baz") -> true
int
is_named_executable (const char* cmd, const char* name)
{
	size_t n = BUFSIZ;
	char buf[BUFSIZ];
	char* base, *end;

	strcpy(buf, cmd);

	base = basename(buf);
	end = strchr(base, ' ');

	if(end)
		n = end-base;

	return 0 == strncmp(base, name, n);
}

// If the PLS variable is set, we format into that
// Otherwise, if EDITOR is set, we try to determine how to use it
// If neither is set we fall back on the default EDITOR_COMMAND.
// This function returns a string using internal static storage.
char*
editor_command ()
{
	static char cmd[BUFSIZ];
	memset(cmd, 0, BUFSIZ);

	const char* env = getenv("PLS");
	if(!env && (env = getenv("EDITOR")))
	{
		// TODO: We don’t want these editor invocations to include --wait,
		// so it might be best to just take the executable path only for those cases.
		if(is_named_executable(env, "mate") || is_named_executable(env, "mate_wait"))
		{
			strcpy(cmd, env);
			strcat(cmd, " --line %d:%d");
			return cmd;
		}
		else if (is_named_executable(env, "subl") || is_named_executable(env, "atom"))
		{
			strcpy(cmd, env);
			strcat(cmd, " %s:%d:%d");
			return cmd;
		}
		else if(is_named_executable(env, "vim") || is_named_executable(env, "vi"))
		{
			strcpy(cmd, env);
			strcat(cmd, " +'call cursor(%d, %d)'");
			return cmd;
		}
		else if(is_named_executable(env, "phpstorm") || is_named_executable(env, "idea"))
		{
			strcpy(cmd, env);
			strcat(cmd, " --line %d \"$PWD/%s\"");
			return cmd;
		}
		else if(is_named_executable(env, "emacs"))
		{
			strcpy(cmd, env);
			strcat(cmd, " +%d:%d");
			return cmd;
		}
	}

	if(!env)
		env = EDITOR_COMMAND;

	strcpy(cmd, env ? env : EDITOR_COMMAND);

	return cmd;
}

#endif
