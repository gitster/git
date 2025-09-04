#include "builtin.h"
#include "branch.h"
#include "gettext.h"
#include "parse-options.h"
#include "sequencer.h"

static int cmd_history_abort(int argc,
			     const char **argv,
			     const char *prefix,
			     struct repository *repo)
{
	const char * const usage[] = {
		N_("git history abort"),
		NULL,
	};
	struct option options[] = {
		OPT_END(),
	};
	struct replay_opts opts = REPLAY_OPTS_INIT;
	int ret;

	argc = parse_options(argc, argv, prefix, options, usage, 0);
	if (argc) {
		ret = error(_("command does not take arguments"));
		goto out;
	}

	opts.action = REPLAY_HISTORY_EDIT;
	ret = sequencer_rollback(repo, &opts);
	if (ret)
		goto out;

	ret = 0;

out:
	replay_opts_release(&opts);
	return ret;
}

static int cmd_history_continue(int argc,
				const char **argv,
				const char *prefix,
				struct repository *repo)
{
	const char * const usage[] = {
		N_("git history continue"),
		NULL,
	};
	struct option options[] = {
		OPT_END(),
	};
	struct replay_opts opts = REPLAY_OPTS_INIT;
	int ret;

	argc = parse_options(argc, argv, prefix, options, usage, 0);
	if (argc) {
		ret = error(_("command does not take arguments"));
		goto out;
	}

	opts.action = REPLAY_HISTORY_EDIT;
	ret = sequencer_continue(repo, &opts);
	if (ret)
		goto out;

	ret = 0;

out:
	replay_opts_release(&opts);
	return ret;
}

static int cmd_history_quit(int argc,
			    const char **argv,
			    const char *prefix,
			    struct repository *repo)
{
	const char * const usage[] = {
		N_("git history quit"),
		NULL,
	};
	struct option options[] = {
		OPT_END(),
	};
	struct replay_opts opts = REPLAY_OPTS_INIT;
	int ret;

	argc = parse_options(argc, argv, prefix, options, usage, 0);
	if (argc) {
		ret = error(_("command does not take arguments"));
		goto out;
	}

	opts.action = REPLAY_HISTORY_EDIT;
	ret = sequencer_remove_state(repo, &opts);
	if (ret)
		goto out;
	remove_branch_state(repo, 0);

	ret = 0;

out:
	replay_opts_release(&opts);
	return ret;
}

int cmd_history(int argc,
		const char **argv,
		const char *prefix,
		struct repository *repo)
{
	const char * const usage[] = {
		N_("git history abort"),
		N_("git history continue"),
		N_("git history quit"),
		NULL,
	};
	parse_opt_subcommand_fn *fn = NULL;
	struct option options[] = {
		OPT_SUBCOMMAND("abort", &fn, cmd_history_abort),
		OPT_SUBCOMMAND("continue", &fn, cmd_history_continue),
		OPT_SUBCOMMAND("quit", &fn, cmd_history_quit),
		OPT_END(),
	};

	argc = parse_options(argc, argv, prefix, options, usage, 0);
	return fn(argc, argv, prefix, repo);
}
