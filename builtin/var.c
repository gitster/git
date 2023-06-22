/*
 * GIT - The information manager from hell
 *
 * Copyright (C) Eric Biederman, 2005
 */
#include "builtin.h"
#include "attr.h"
#include "config.h"
#include "editor.h"
#include "ident.h"
#include "pager.h"
#include "refs.h"

static const char var_usage[] = "git var (-l | <variable>)";

static const char *editor(int flag)
{
	return git_editor();
}

static const char *sequence_editor(int flag)
{
	return git_sequence_editor();
}

static const char *pager(int flag)
{
	const char *pgm = git_pager(1);

	if (!pgm)
		pgm = "cat";
	return pgm;
}

static const char *default_branch(int flag)
{
	return git_default_branch_name(1);
}

static const char *shell_path(int flag)
{
	return SHELL_PATH;
}

static const char *git_attr_val_system(int flag)
{
	if (git_attr_system()) {
		char *file = xstrdup(git_etc_gitattributes());
		normalize_path_copy(file, file);
		return file;
	}
	return NULL;
}

static const char *git_attr_val_global(int flag)
{
	char *file = xstrdup(get_home_gitattributes());
	if (file) {
		normalize_path_copy(file, file);
		return file;
	}
	return NULL;
}

struct git_var {
	const char *name;
	const char *(*read)(int);
	int free;
};
static struct git_var git_vars[] = {
	{ "GIT_COMMITTER_IDENT", git_committer_info, 0 },
	{ "GIT_AUTHOR_IDENT",   git_author_info, 0 },
	{ "GIT_EDITOR", editor, 0 },
	{ "GIT_SEQUENCE_EDITOR", sequence_editor, 0 },
	{ "GIT_PAGER", pager, 0 },
	{ "GIT_DEFAULT_BRANCH", default_branch, 0 },
	{ "GIT_SHELL_PATH", shell_path, 0 },
	{ "GIT_ATTR_SYSTEM", git_attr_val_system, 1 },
	{ "GIT_ATTR_GLOBAL", git_attr_val_global, 1 },
	{ "", NULL },
};

static void list_vars(void)
{
	struct git_var *ptr;
	const char *val;

	for (ptr = git_vars; ptr->read; ptr++)
		if ((val = ptr->read(0))) {
			printf("%s=%s\n", ptr->name, val);
			if (ptr->free)
				free((void *)val);
		}
}

static const struct git_var *get_git_var(const char *var)
{
	struct git_var *ptr;
	for (ptr = git_vars; ptr->read; ptr++) {
		if (strcmp(var, ptr->name) == 0) {
			return ptr;
		}
	}
	return NULL;
}

static int show_config(const char *var, const char *value, void *cb)
{
	if (value)
		printf("%s=%s\n", var, value);
	else
		printf("%s\n", var);
	return git_default_config(var, value, cb);
}

int cmd_var(int argc, const char **argv, const char *prefix UNUSED)
{
	const struct git_var *git_var;
	const char *val;

	if (argc != 2)
		usage(var_usage);

	if (strcmp(argv[1], "-l") == 0) {
		git_config(show_config, NULL);
		list_vars();
		return 0;
	}
	git_config(git_default_config, NULL);

	git_var = get_git_var(argv[1]);
	if (!git_var)
		usage(var_usage);

	val = git_var->read(IDENT_STRICT);
	if (!val)
		return 1;

	printf("%s\n", val);
	if (git_var->free)
		free((void *)val);

	return 0;
}
