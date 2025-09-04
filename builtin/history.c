#include "builtin.h"
#include "branch.h"
#include "commit.h"
#include "commit-reach.h"
#include "config.h"
#include "environment.h"
#include "gettext.h"
#include "hex.h"
#include "object-name.h"
#include "parse-options.h"
#include "refs.h"
#include "reset.h"
#include "revision.h"
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

static int collect_commits(struct repository *repo,
			   struct commit *old_commit,
			   struct commit *new_commit,
			   struct strvec *out)
{
	struct setup_revision_opt revision_opts = {
		.assume_dashdash = 1,
	};
	struct strvec revisions = STRVEC_INIT;
	struct commit_list *from_list = NULL;
	struct commit *child;
	struct rev_info rev = { 0 };
	int ret;

	/*
	 * Check that the old commit actually is an ancestor of HEAD. If not
	 * the whole request becomes nonsensical.
	*/
	if (old_commit) {
		commit_list_insert(old_commit, &from_list);
		if (!repo_is_descendant_of(repo, new_commit, from_list)) {
			ret = error(_("commit must be reachable from current HEAD commit"));
			goto out;
		}
	}

	repo_init_revisions(repo, &rev, NULL);
	strvec_push(&revisions, "");
	strvec_push(&revisions, oid_to_hex(&new_commit->object.oid));
	if (old_commit)
		strvec_pushf(&revisions, "^%s", oid_to_hex(&old_commit->object.oid));
	if (setup_revisions(revisions.nr, revisions.v, &rev, &revision_opts) != 1 ||
	    prepare_revision_walk(&rev)) {
		ret = error(_("revision walk setup failed"));
		goto out;
	}

	while ((child = get_revision(&rev))) {
		if (old_commit && !child->parents)
			BUG("revision walk did not find child commit");
		if (child->parents && child->parents->next) {
			ret = error(_("cannot rearrange commit history with merges"));
			goto out;
		}

		strvec_push(out, oid_to_hex(&child->object.oid));

		if (child->parents && old_commit &&
		    commit_list_contains(old_commit, child->parents))
			break;
	}

	/*
	 * Revisions are in newest-order-first. We have to reverse the
	 * array though so that we pick the oldest commits first.
	 */
	for (size_t i = 0, j = out->nr - 1; i < j; i++, j--)
		SWAP(out->v[i], out->v[j]);

	ret = 0;

out:
	free_commit_list(from_list);
	strvec_clear(&revisions);
	release_revisions(&rev);
	reset_revision_walk();
	return ret;
}

static int apply_commits(struct repository *repo,
			 const struct strvec *commits,
			 struct commit *head,
			 struct commit *base,
			 const char *action)
{
	struct setup_revision_opt revision_opts = {
		.assume_dashdash = 1,
	};
	struct replay_opts replay_opts = REPLAY_OPTS_INIT;
	struct reset_head_opts reset_opts = { 0 };
	struct object_id root_commit;
	struct strvec args = STRVEC_INIT;
	struct strbuf buf = STRBUF_INIT;
	char hex[GIT_MAX_HEXSZ + 1];
	int ref_flags, ret;

	/*
	 * We have performed all safety checks, so we now prepare
	 * replaying the commits.
	*/
	replay_opts.action = REPLAY_HISTORY_EDIT;
	sequencer_init_config(&replay_opts);
	replay_opts.quiet = 1;
	replay_opts.skip_commit_summary = 1;
	if (!replay_opts.strategy && replay_opts.default_strategy) {
		replay_opts.strategy = replay_opts.default_strategy;
		replay_opts.default_strategy = NULL;
	}

	strvec_push(&args, "");
	strvec_pushv(&args, commits->v);

	replay_opts.revs = xmalloc(sizeof(*replay_opts.revs));
	repo_init_revisions(repo, replay_opts.revs, NULL);
	replay_opts.revs->no_walk = 1;
	replay_opts.revs->unsorted_input = 1;
	if (setup_revisions(args.nr, args.v, replay_opts.revs,
			    &revision_opts) != 1) {
		ret = error(_("setting up revisions failed"));
		goto out;
	}

	/*
	 * If we're dropping the root commit we first need to create
	 * a new empty root. We then instruct the seqencer machinery to
	 * squash that root commit with the first commit we're picking
	 * onto it.
	 */
	if (!base->parents) {
		if (commit_tree("", 0, repo->hash_algo->empty_tree, NULL,
				&root_commit, NULL, NULL) < 0) {
			ret = error(_("Could not create new root commit"));
			goto out;
		}

		replay_opts.squash_onto = root_commit;
		replay_opts.have_squash_onto = 1;
		reset_opts.oid = &root_commit;
	} else {
		reset_opts.oid = &base->parents->item->object.oid;
	}

	replay_opts.restore_head_target =
		xstrdup_or_null(refs_resolve_ref_unsafe(get_main_ref_store(repo),
							"HEAD", 0, NULL, &ref_flags));
	if (!(ref_flags & REF_ISSYMREF))
		FREE_AND_NULL(replay_opts.restore_head_target);

	/*
	 * Perform a hard-reset to the parent of our commit that is to
	 * be dropped. This is the new base onto which we'll pick all
	 * the descendants.
	 */
	strbuf_addf(&buf, "%s (start): checkout %s", action,
		    oid_to_hex_r(hex, reset_opts.oid));
	reset_opts.orig_head = &head->object.oid;
	reset_opts.flags = RESET_HEAD_DETACH | RESET_ORIG_HEAD;
	reset_opts.head_msg = buf.buf;
	reset_opts.default_reflog_action = action;
	if (reset_head(repo, &reset_opts) < 0) {
		ret = error(_("could not switch to %s"), oid_to_hex(reset_opts.oid));
		goto out;
	}

	ret = sequencer_pick_revisions(repo, &replay_opts);
	if (ret < 0) {
		ret = error(_("could not pick commits"));
		goto out;
	} else if (ret > 0) {
		/*
		 * A positive return value indicates we've got a merge
		 * conflict. Bail out, but don't print a message as
		 * `sequencer_pick_revisions()` already printed enough
		 * information.
		 */
		ret = -1;
		goto out;
	}

	ret = 0;

out:
	replay_opts_release(&replay_opts);
	strbuf_release(&buf);
	strvec_clear(&args);
	return ret;
}

static int cmd_history_drop(int argc,
			    const char **argv,
			    const char *prefix,
			    struct repository *repo)
{
	const char * const usage[] = {
		N_("git history drop <commit>"),
		NULL,
	};
	struct option options[] = {
		OPT_END(),
	};
	struct commit *commit_to_drop, *head;
	struct strvec commits = STRVEC_INIT;
	struct strbuf buf = STRBUF_INIT;
	int ret;

	argc = parse_options(argc, argv, prefix, options, usage, 0);
	if (argc != 1) {
		ret = error(_("command expects a single revision"));
		goto out;
	}
	repo_config(repo, git_default_config, NULL);

	commit_to_drop = lookup_commit_reference_by_name(argv[0]);
	if (!commit_to_drop) {
		ret = error(_("commit to be dropped cannot be found: %s"), argv[0]);
		goto out;
	}
	if (commit_to_drop->parents && commit_to_drop->parents->next) {
		ret = error(_("commit to be dropped must not be a merge commit"));
		goto out;
	}

	head = lookup_commit_reference_by_name("HEAD");
	if (!head) {
		ret = error(_("could not resolve HEAD to a commit"));
		goto out;
	}

	if (oideq(&commit_to_drop->object.oid, &head->object.oid)) {
		/*
		 * If we want to drop the tip of the current branch we don't
		 * have to perform any rebase at all. Instead, we simply
		 * perform a hard reset to the parent commit.
		 */
		struct reset_head_opts reset_opts = {
			.orig_head = &head->object.oid,
			.flags = RESET_ORIG_HEAD,
			.default_reflog_action = "drop",
		};
		char hex[GIT_MAX_HEXSZ + 1];

		if (!commit_to_drop->parents) {
			ret = error(_("cannot drop the only commit on this branch"));
			goto out;
		}

		oid_to_hex_r(hex, &commit_to_drop->parents->item->object.oid);
		strbuf_addf(&buf, "drop (start): checkout %s", hex);
		reset_opts.oid = &commit_to_drop->parents->item->object.oid;
		reset_opts.head_msg = buf.buf;

		if (reset_head(repo, &reset_opts) < 0) {
			ret = error(_("could not switch to %s"), hex);
			goto out;
		}
	} else {
		/*
		 * Prepare a revision walk from old commit to the commit that is
		 * about to be dropped. This serves three purposes:
		 *
		 *   - We verify that the history doesn't contain any merges.
		 *     For now, merges aren't yet handled by us.
		 *
		 *   - We need to find the child of the commit-to-be-dropped.
		 *     This child is what will be adopted by the parent of the
		 *     commit that we are about to drop.
		 *
		 *   - We compute the list of commits-to-be-picked.
		 */
		ret = collect_commits(repo, commit_to_drop, head, &commits);
		if (ret < 0)
			goto out;

		ret = apply_commits(repo, &commits, head, commit_to_drop, "drop");
		if (ret < 0)
			goto out;
	}

	ret = 0;

out:
	strvec_clear(&commits);
	strbuf_release(&buf);
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
		N_("git history drop <commit>"),
		NULL,
	};
	parse_opt_subcommand_fn *fn = NULL;
	struct option options[] = {
		OPT_SUBCOMMAND("abort", &fn, cmd_history_abort),
		OPT_SUBCOMMAND("continue", &fn, cmd_history_continue),
		OPT_SUBCOMMAND("quit", &fn, cmd_history_quit),
		OPT_SUBCOMMAND("drop", &fn, cmd_history_drop),
		OPT_END(),
	};

	argc = parse_options(argc, argv, prefix, options, usage, 0);
	return fn(argc, argv, prefix, repo);
}
