#include "builtin.h"
#include "parse-options.h"

static int repo_info(int argc UNUSED,
		     const char **argv UNUSED,
		     const char *prefix UNUSED,
		     struct repository *repo UNUSED)
{
	return 0;
}

int cmd_repo(int argc,
	     const char **argv,
	     const char *prefix,
	     struct repository *repo)
{
	parse_opt_subcommand_fn *fn = NULL;
	const char *const repo_usage[] = {
		"git repo info",
		NULL
	};
	struct option options[] = {
		OPT_SUBCOMMAND("info", &fn, repo_info),
		OPT_END()
	};

	argc = parse_options(argc, argv, prefix, options, repo_usage, 0);

	if (fn) {
		return fn(argc, argv, prefix, repo);
	} else {
		if (argc) {
			error(_("unknown subcommand: `%s'"), argv[0]);
			usage_with_options(repo_usage, options);
		}
		return 1;
	}
}
