/* Required for `comment_line_str`. */
#define USE_THE_REPOSITORY_VARIABLE

#include "builtin.h"
#include "branch.h"
#include "cache-tree.h"
#include "commit.h"
#include "commit-reach.h"
#include "config.h"
#include "editor.h"
#include "environment.h"
#include "gettext.h"
#include "hex.h"
#include "object-name.h"
#include "parse-options.h"
#include "path.h"
#include "pathspec.h"
#include "read-cache-ll.h"
#include "refs.h"
#include "reset.h"
#include "revision.h"
#include "run-command.h"
#include "sequencer.h"
#include "sparse-index.h"

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

static void replace_commits(struct strvec *commits,
			    const struct object_id *commit_to_replace,
			    const struct object_id *replacements,
			    size_t replacements_nr)
{
	char commit_to_replace_oid[GIT_MAX_HEXSZ + 1];
	struct strvec replacement_oids = STRVEC_INIT;
	bool found = false;
	size_t i;

	oid_to_hex_r(commit_to_replace_oid, commit_to_replace);
	for (i = 0; i < replacements_nr; i++)
		strvec_push(&replacement_oids, oid_to_hex(&replacements[i]));

	for (i = 0; i < commits->nr; i++) {
		if (strcmp(commits->v[i], commit_to_replace_oid))
			continue;
		strvec_splice(commits, i, 1, replacement_oids.v, replacement_oids.nr);
		found = true;
		break;
	}
	if (!found)
		BUG("could not find commit to replace");

	strvec_clear(&replacement_oids);
}

static int apply_commits(struct repository *repo,
			 const struct strvec *commits,
			 struct commit *head,
			 struct commit *base,
			 const struct oidmap *rewritten_commits,
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
	replay_opts.old_oid_mappings = rewritten_commits;

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

		ret = apply_commits(repo, &commits, head, commit_to_drop,
				    NULL, "drop");
		if (ret < 0)
			goto out;
	}

	ret = 0;

out:
	strvec_clear(&commits);
	strbuf_release(&buf);
	return ret;
}

static int cmd_history_reorder(int argc,
			       const char **argv,
			       const char *prefix,
			       struct repository *repo)
{
	const char * const usage[] = {
		N_("git history reorder <commit> (--before=<following-commit>|--after=<preceding-commit>)"),
		NULL,
	};
	const char *before = NULL, *after = NULL;
	struct option options[] = {
		OPT_STRING(0, "before", &before, N_("commit"), N_("reorder before this commit")),
		OPT_STRING(0, "after", &after, N_("commit"), N_("reorder after this commit")),
		OPT_END(),
	};
	struct commit *commit_to_reorder, *head, *anchor, *old;
	struct strvec commits = STRVEC_INIT;
	struct object_id replacement[2];
	struct commit_list *list = NULL;
	int ret;

	argc = parse_options(argc, argv, prefix, options, usage, 0);
	if (argc != 1)
		die(_("command expects a single revision"));
	if (!before && !after)
		die(_("exactly one option of 'before' or 'after' must be given"));
	die_for_incompatible_opt2(!!before, "before", !!after, "after");

	repo_config(repo, git_default_config, NULL);

	commit_to_reorder = lookup_commit_reference_by_name(argv[0]);
	if (!commit_to_reorder)
		die(_("commit to be reordered cannot be found: %s"), argv[0]);
	if (commit_to_reorder->parents && commit_to_reorder->parents->next)
		die(_("commit to be reordered must not be a merge commit"));

	anchor = lookup_commit_reference_by_name(before ? before : after);
	if (!commit_to_reorder)
		die(_("anchor commit cannot be found: %s"), before ? before : after);

	if (oideq(&commit_to_reorder->object.oid, &anchor->object.oid))
		die(_("commit to reorder and anchor must not be the same"));

	head = lookup_commit_reference_by_name("HEAD");
	if (!head)
		die(_("could not resolve HEAD to a commit"));

	commit_list_append(commit_to_reorder, &list);
	if (!repo_is_descendant_of(repo, commit_to_reorder, list))
		die(_("reordered commit must be reachable from current HEAD commit"));

	/*
	 * There is no requirement for the user to have either one of the
	 * provided commits be the parent or child. We thus have to figure out
	 * ourselves which one is which.
	*/
	if (repo_is_descendant_of(repo, anchor, list))
		old = commit_to_reorder;
	else
		old = anchor;

	/*
	 * Select the whole range of commits, including the boundary commit
	 * itself. In case the old commit is the root commit we simply pass no
	 * boundary.
	*/
	ret = collect_commits(repo, old->parents ? old->parents->item : NULL,
			      head, &commits);
	if (ret < 0)
		goto out;

	/*
	 * Perform the reordering of commits in the strvec. This is done by:
	 *
	 *   - Deleting the to-be-reordered commit from the range of commits.
	 *
	 *   - Replacing the anchor commit with the anchor commit plus the
	 *     to-be-reordered commit.
	 */
	if (before) {
		replacement[0] = commit_to_reorder->object.oid;
		replacement[1] = anchor->object.oid;
	} else {
		replacement[0] = anchor->object.oid;
		replacement[1] = commit_to_reorder->object.oid;
	}
	replace_commits(&commits, &commit_to_reorder->object.oid, NULL, 0);
	replace_commits(&commits, &anchor->object.oid, replacement, ARRAY_SIZE(replacement));

	ret = apply_commits(repo, &commits, head, old, NULL, "reorder");
	if (ret < 0)
		goto out;

	ret = 0;

out:
	free_commit_list(list);
	strvec_clear(&commits);
	return ret;
}

static void change_data_free(void *util, const char *str UNUSED)
{
	struct wt_status_change_data *d = util;
	free(d->rename_source);
	free(d);
}

static int fill_commit_message(struct repository *repo,
			       const struct object_id *old_tree,
			       const struct object_id *new_tree,
			       const char *default_message,
			       const char *provided_message,
			       const char *action,
			       struct strbuf *out)
{
	if (!provided_message) {
		const char *path = git_path_commit_editmsg();
		const char *hint =
			_("Please enter the commit message for the %s changes. Lines starting\n"
			  "with '%s' will be kept; you may remove them yourself if you want to.\n");
		int verbose = 1;

		strbuf_addstr(out, default_message);
		strbuf_addch(out, '\n');
		strbuf_commented_addf(out, comment_line_str, hint, action, comment_line_str);
		write_file_buf(path, out->buf, out->len);

		repo_config_get_bool(repo, "commit.verbose", &verbose);
		if (verbose) {
			struct wt_status s;

			wt_status_prepare(repo, &s);
			FREE_AND_NULL(s.branch);
			s.ahead_behind_flags = AHEAD_BEHIND_QUICK;
			s.commit_template = 1;
			s.colopts = 0;
			s.display_comment_prefix = 1;
			s.hints = 0;
			s.use_color = 0;
			s.whence = FROM_COMMIT;
			s.committable = 1;

			s.fp = fopen(git_path_commit_editmsg(), "a");
			if (!s.fp)
				return error_errno(_("could not open '%s'"), git_path_commit_editmsg());

			wt_status_collect_changes_trees(&s, old_tree, new_tree);
			wt_status_print(&s);
			wt_status_collect_free_buffers(&s);
			string_list_clear_func(&s.change, change_data_free);
		}

		strbuf_reset(out);
		if (launch_editor(path, out, NULL)) {
			fprintf(stderr, _("Please supply the message using the -m option.\n"));
			return -1;
		}
		strbuf_stripspace(out, comment_line_str);
	} else {
		strbuf_addstr(out, provided_message);
	}

	cleanup_message(out, COMMIT_MSG_CLEANUP_ALL, 0);

	if (!out->len) {
		fprintf(stderr, _("Aborting commit due to empty commit message.\n"));
		return -1;
	}

	return 0;
}

static int split_commit(struct repository *repo,
			struct commit *original_commit,
			struct pathspec *pathspec,
			const char *commit_message,
			struct object_id *out)
{
	struct interactive_options interactive_opts = INTERACTIVE_OPTIONS_INIT;
	struct strbuf index_file = STRBUF_INIT, split_message = STRBUF_INIT;
	struct child_process read_tree_cmd = CHILD_PROCESS_INIT;
	struct index_state index = INDEX_STATE_INIT(repo);
	struct object_id original_commit_tree_oid, parent_tree_oid;
	const char *original_message, *original_body, *ptr;
	char original_commit_oid[GIT_MAX_HEXSZ + 1];
	char *original_author = NULL;
	struct commit_list *parents = NULL;
	struct commit *first_commit;
	struct tree *split_tree;
	size_t len;
	int ret;

	if (original_commit->parents)
		parent_tree_oid = *get_commit_tree_oid(original_commit->parents->item);
	else
		oidcpy(&parent_tree_oid, repo->hash_algo->empty_tree);
	original_commit_tree_oid = *get_commit_tree_oid(original_commit);

	/*
	 * Construct the first commit. This is done by taking the original
	 * commit parent's tree and selectively patching changes from the diff
	 * between that parent and its child.
	 */
	repo_git_path_replace(repo, &index_file, "%s", "history-split.index");

	read_tree_cmd.git_cmd = 1;
	strvec_pushf(&read_tree_cmd.env, "GIT_INDEX_FILE=%s", index_file.buf);
	strvec_push(&read_tree_cmd.args, "read-tree");
	strvec_push(&read_tree_cmd.args, oid_to_hex(&parent_tree_oid));
	ret = run_command(&read_tree_cmd);
	if (ret < 0)
		goto out;

	ret = read_index_from(&index, index_file.buf, repo->gitdir);
	if (ret < 0) {
		ret = error(_("failed reading temporary index"));
		goto out;
	}

	oid_to_hex_r(original_commit_oid, &original_commit->object.oid);
	ret = run_add_p_index(repo, &index, index_file.buf, &interactive_opts,
			      original_commit_oid, pathspec);
	if (ret < 0)
		goto out;

	split_tree = write_in_core_index_as_tree(repo, &index);
	if (!split_tree) {
		ret = error(_("failed split tree"));
		goto out;
	}

	unlink(index_file.buf);

	/*
	 * We disallow the cases where either the split-out commit or the
	 * original commit would become empty. Consequently, if we see that the
	 * new tree ID matches either of those trees we abort.
	 */
	if (oideq(&split_tree->object.oid, &parent_tree_oid)) {
		ret = error(_("split commit is empty"));
		goto out;
	} else if (oideq(&split_tree->object.oid, &original_commit_tree_oid)) {
		ret = error(_("split commit tree matches original commit"));
		goto out;
	}

	/* We retain authorship of the original commit. */
	original_message = repo_logmsg_reencode(repo, original_commit, NULL, NULL);
	ptr = find_commit_header(original_message, "author", &len);
	if (ptr)
		original_author = xmemdupz(ptr, len);

	ret = fill_commit_message(repo, &parent_tree_oid, &split_tree->object.oid,
				  "", commit_message, "split-out", &split_message);
	if (ret < 0)
		goto out;

	ret = commit_tree(split_message.buf, split_message.len, &split_tree->object.oid,
			  original_commit->parents, &out[0], original_author, NULL);
	if (ret < 0) {
		ret = error(_("failed writing split-out commit"));
		goto out;
	}

	/*
	 * The second commit is much simpler to construct, as we can simply use
	 * the original commit details, except that we adjust its parent to be
	 * the newly split-out commit.
	 */
	find_commit_subject(original_message, &original_body);
	first_commit = lookup_commit_reference(repo, &out[0]);
	commit_list_append(first_commit, &parents);

	ret = commit_tree(original_body, strlen(original_body), &original_commit_tree_oid,
			  parents, &out[1], original_author, NULL);
	if (ret < 0) {
		ret = error(_("failed writing second commit"));
		goto out;
	}

	ret = 0;

out:
	if (index_file.len)
		unlink(index_file.buf);
	strbuf_release(&split_message);
	strbuf_release(&index_file);
	free_commit_list(parents);
	free(original_author);
	release_index(&index);
	return ret;
}

static int cmd_history_split(int argc,
			     const char **argv,
			     const char *prefix,
			     struct repository *repo)
{
	const char * const usage[] = {
		N_("git history split [<options>] <commit>"),
		NULL,
	};
	const char *commit_message = NULL;
	struct option options[] = {
		OPT_STRING('m', "message", &commit_message, N_("message"), N_("commit message")),
		OPT_END(),
	};
	struct oidmap rewritten_commits = OIDMAP_INIT;
	struct commit *original_commit, *head;
	struct strvec commits = STRVEC_INIT;
	struct commit_list *list = NULL;
	struct object_id split_commits[2];
	struct replay_oid_mapping mapping[2] = { 0 };
	struct pathspec pathspec = { 0 };
	int ret;

	argc = parse_options(argc, argv, prefix, options, usage, 0);
	if (argc < 1) {
		ret = error(_("command expects a revision"));
		goto out;
	}
	repo_config(repo, git_default_config, NULL);

	original_commit = lookup_commit_reference_by_name(argv[0]);
	if (!original_commit) {
		ret = error(_("commit to be split cannot be found: %s"), argv[0]);
		goto out;
	}

	if (original_commit->parents && original_commit->parents->next) {
		ret = error(_("commit to be split must not be a merge commit"));
		goto out;
	}

	head = lookup_commit_reference_by_name("HEAD");
	if (!head) {
		ret = error(_("could not resolve HEAD to a commit"));
		goto out;
	}

	commit_list_append(original_commit, &list);
	if (!repo_is_descendant_of(repo, original_commit, list)) {
		ret = error (_("split commit must be reachable from current HEAD commit"));
		goto out;
	}

	parse_pathspec(&pathspec, 0,
		       PATHSPEC_PREFER_FULL | PATHSPEC_SYMLINK_LEADING_PATH | PATHSPEC_PREFIX_ORIGIN,
		       prefix, argv + 1);

	/*
	 * Collect the list of commits that we'll have to reapply now already.
	 * This ensures that we'll abort early on in case the range of commits
	 * contains merges, which we do not yet handle.
	 */
	ret = collect_commits(repo, original_commit->parents ? original_commit->parents->item : NULL,
			      head, &commits);
	if (ret < 0)
		goto out;

	/*
	 * Then we split up the commit and replace the original commit with the
	 * new new ones.
	 */
	ret = split_commit(repo, original_commit, &pathspec,
			   commit_message, split_commits);
	if (ret < 0)
		goto out;

	mapping[0].entry.oid = split_commits[0];
	mapping[0].rewritten_oid = original_commit->object.oid;
	oidmap_put(&rewritten_commits, &mapping[0]);
	mapping[1].entry.oid = split_commits[1];
	mapping[1].rewritten_oid = original_commit->object.oid;
	oidmap_put(&rewritten_commits, &mapping[1]);

	replace_commits(&commits, &original_commit->object.oid,
			split_commits, ARRAY_SIZE(split_commits));

	ret = apply_commits(repo, &commits, head, original_commit,
			    &rewritten_commits, "split");
	if (ret < 0)
		goto out;

	ret = 0;

out:
	oidmap_clear(&rewritten_commits, 0);
	clear_pathspec(&pathspec);
	strvec_clear(&commits);
	free_commit_list(list);
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
		N_("git history reorder <commit> (--before=<following-commit>|--after=<preceding-commit>)"),
		N_("git history split [<options>] <commit> [--] [<pathspec>...]"),
		NULL,
	};
	parse_opt_subcommand_fn *fn = NULL;
	struct option options[] = {
		OPT_SUBCOMMAND("abort", &fn, cmd_history_abort),
		OPT_SUBCOMMAND("continue", &fn, cmd_history_continue),
		OPT_SUBCOMMAND("quit", &fn, cmd_history_quit),
		OPT_SUBCOMMAND("drop", &fn, cmd_history_drop),
		OPT_SUBCOMMAND("reorder", &fn, cmd_history_reorder),
		OPT_SUBCOMMAND("split", &fn, cmd_history_split),
		OPT_END(),
	};

	argc = parse_options(argc, argv, prefix, options, usage, 0);
	return fn(argc, argv, prefix, repo);
}
