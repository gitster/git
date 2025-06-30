#include "git-compat-util.h"
#include "last-modified.h"
#include "hex.h"
#include "quote.h"
#include "config.h"
#include "object-name.h"
#include "parse-options.h"
#include "builtin.h"

static void show_entry(const char *path, const struct commit *commit, void *d)
{
	struct last_modified *lm = d;

	if (commit->object.flags & BOUNDARY)
		putchar('^');
	printf("%s\t", oid_to_hex(&commit->object.oid));

	if (lm->rev.diffopt.line_termination)
		write_name_quoted(path, stdout, '\n');
	else
		printf("%s%c", path, '\0');

	fflush(stdout);
}

int cmd_last_modified(int argc,
		   const char **argv,
		   const char *prefix,
		   struct repository *repo)
{
	struct last_modified lm;

	repo_config(repo, git_default_config, NULL);

	if (last_modified_init(&lm, repo, prefix, argc, argv))
		die(_("error setting up last-modified traversal"));

	if (last_modified_run(&lm, show_entry, &lm) < 0)
		die(_("error running last-modified traversal"));

	last_modified_release(&lm);

	return 0;
}
