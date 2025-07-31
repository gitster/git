#include "git-compat-util.h"
#include "builtin.h"
#include "commit.h"
#include "config.h"
#include "diff.h"
#include "diffcore.h"
#include "hashmap.h"
#include "hex.h"
#include "log-tree.h"
#include "object-name.h"
#include "object.h"
#include "parse-options.h"
#include "quote.h"
#include "repository.h"
#include "revision.h"

struct last_modified_entry {
	struct hashmap_entry hashent;
	struct object_id oid;
	const char path[FLEX_ARRAY];
};

static int last_modified_entry_hashcmp(const void *unused UNUSED,
				       const struct hashmap_entry *hent1,
				       const struct hashmap_entry *hent2,
				       const void *path)
{
	const struct last_modified_entry *ent1 =
		container_of(hent1, const struct last_modified_entry, hashent);
	const struct last_modified_entry *ent2 =
		container_of(hent2, const struct last_modified_entry, hashent);
	return strcmp(ent1->path, path ? path : ent2->path);
}

struct last_modified {
	struct hashmap paths;
	struct rev_info rev;
	int recursive;
	int tree_in_recursive;
};

static void last_modified_release(struct last_modified *lm)
{
	hashmap_clear_and_free(&lm->paths, struct last_modified_entry, hashent);
	release_revisions(&lm->rev);
}

struct last_modified_callback_data {
	struct last_modified *lm;
	struct commit *commit;
};

static void add_path_from_diff(struct diff_queue_struct *q,
			       struct diff_options *opt UNUSED, void *data)
{
	struct last_modified *lm = data;

	for (int i = 0; i < q->nr; i++) {
		struct diff_filepair *p = q->queue[i];
		struct last_modified_entry *ent;
		const char *path = p->two->path;

		FLEX_ALLOC_STR(ent, path, path);
		oidcpy(&ent->oid, &p->two->oid);
		hashmap_entry_init(&ent->hashent, strhash(ent->path));
		hashmap_add(&lm->paths, &ent->hashent);
	}
}

static int populate_paths_from_revs(struct last_modified *lm)
{
	int num_interesting = 0;
	struct diff_options diffopt;

	memcpy(&diffopt, &lm->rev.diffopt, sizeof(diffopt));
	copy_pathspec(&diffopt.pathspec, &lm->rev.diffopt.pathspec);
	/*
	 * Use a callback to populate the paths from revs
	 */
	diffopt.output_format = DIFF_FORMAT_CALLBACK;
	diffopt.format_callback = add_path_from_diff;
	diffopt.format_callback_data = lm;

	for (size_t i = 0; i < lm->rev.pending.nr; i++) {
		struct object_array_entry *obj = lm->rev.pending.objects + i;

		if (obj->item->flags & UNINTERESTING)
			continue;

		if (num_interesting++)
			return error(_("last-modified can only operate on one tree at a time"));

		diff_tree_oid(lm->rev.repo->hash_algo->empty_tree,
			      &obj->item->oid, "", &diffopt);
		diff_flush(&diffopt);
	}
	diff_free(&diffopt);

	return 0;
}

static void last_modified_emit(struct last_modified *lm,
			       const char *path, const struct commit *commit)

{
	if (commit->object.flags & BOUNDARY)
		putchar('^');
	printf("%s\t", oid_to_hex(&commit->object.oid));

	if (lm->rev.diffopt.line_termination)
		write_name_quoted(path, stdout, '\n');
	else
		printf("%s%c", path, '\0');

	fflush(stdout);
}

static void mark_path(const char *path, const struct object_id *oid,
		      struct last_modified_callback_data *data)
{
	struct last_modified_entry *ent;

	/* Is it even a path that we are interested in? */
	ent = hashmap_get_entry_from_hash(&data->lm->paths, strhash(path), path,
					  struct last_modified_entry, hashent);
	if (!ent)
		return;

	/*
	 * Is it arriving at a version of interest, or is it from a side branch
	 * which did not contribute to the final state?
	 */
	if (!oideq(oid, &ent->oid))
		return;

	last_modified_emit(data->lm, path, data->commit);

	hashmap_remove(&data->lm->paths, &ent->hashent, path);
	free(ent);
}

static void last_modified_diff(struct diff_queue_struct *q,
			       struct diff_options *opt UNUSED, void *cbdata)
{
	struct last_modified_callback_data *data = cbdata;

	for (int i = 0; i < q->nr; i++) {
		struct diff_filepair *p = q->queue[i];
		switch (p->status) {
		case DIFF_STATUS_DELETED:
			/*
			 * There's no point in feeding a deletion, as it could
			 * not have resulted in our current state, which
			 * actually has the file.
			 */
			break;

		default:
			/*
			 * Otherwise, we care only that we somehow arrived at
			 * a final oid state. Note that this covers some
			 * potentially controversial areas, including:
			 *
			 *  1. A rename or copy will be found, as it is the
			 *     first time the content has arrived at the given
			 *     path.
			 *
			 *  2. Even a non-content modification like a mode or
			 *     type change will trigger it.
			 *
			 * We take the inclusive approach for now, and find
			 * anything which impacts the path. Options to tweak
			 * the behavior (e.g., to "--follow" the content across
			 * renames) can come later.
			 */
			mark_path(p->two->path, &p->two->oid, data);
			break;
		}
	}
}

static int last_modified_run(struct last_modified *lm)
{
	struct last_modified_callback_data data = { .lm = lm };

	lm->rev.diffopt.output_format = DIFF_FORMAT_CALLBACK;
	lm->rev.diffopt.format_callback = last_modified_diff;
	lm->rev.diffopt.format_callback_data = &data;

	prepare_revision_walk(&lm->rev);

	while (hashmap_get_size(&lm->paths)) {
		data.commit = get_revision(&lm->rev);
		if (!data.commit)
			break;

		if (data.commit->object.flags & BOUNDARY) {
			diff_tree_oid(lm->rev.repo->hash_algo->empty_tree,
				      &data.commit->object.oid, "",
				      &lm->rev.diffopt);
			diff_flush(&lm->rev.diffopt);
		} else {
			log_tree_commit(&lm->rev, data.commit);
		}
	}

	return 0;
}

static int last_modified_init(struct last_modified *lm, struct repository *r,
			      const char *prefix, int argc, const char **argv)
{
	hashmap_init(&lm->paths, last_modified_entry_hashcmp, NULL, 0);

	repo_init_revisions(r, &lm->rev, prefix);
	lm->rev.def = "HEAD";
	lm->rev.combine_merges = 1;
	lm->rev.show_root_diff = 1;
	lm->rev.boundary = 1;
	lm->rev.no_commit_id = 1;
	lm->rev.diff = 1;
	lm->rev.diffopt.flags.recursive = lm->recursive || lm->tree_in_recursive;
	lm->rev.diffopt.flags.tree_in_recursive = lm->tree_in_recursive;

	if ((argc = setup_revisions(argc, argv, &lm->rev, NULL)) > 1) {
		error(_("unknown last-modified argument: %s"), argv[1]);
		return argc;
	}

	if (populate_paths_from_revs(lm) < 0)
		return error(_("unable to setup last-modified"));

	return 0;
}

int cmd_last_modified(int argc, const char **argv, const char *prefix,
		      struct repository *repo)
{
	int ret;
	struct last_modified lm;

	const char * const last_modified_usage[] = {
		N_("git last-modified [-r] [-t] "
		   "[<revision-range>] [[--] <path>...]"),
		NULL
	};

	struct option last_modified_options[] = {
		OPT_BOOL('r', "recursive", &lm.recursive,
			 N_("recurse into subtrees")),
		OPT_BOOL('t', "tree-in-recursive", &lm.tree_in_recursive,
			 N_("recurse into subtrees and include the tree entries too")),
		OPT_END()
	};

	memset(&lm, 0, sizeof(lm));

	argc = parse_options(argc, argv, prefix, last_modified_options,
			     last_modified_usage,
			     PARSE_OPT_KEEP_ARGV0 | PARSE_OPT_KEEP_UNKNOWN_OPT);

	repo_config(repo, git_default_config, NULL);

	if ((ret = last_modified_init(&lm, repo, prefix, argc, argv))) {
		if (ret > 0)
			usage_with_options(last_modified_usage,
					   last_modified_options);
		goto out;
	}

	if ((ret = last_modified_run(&lm)))
		goto out;

out:
	last_modified_release(&lm);

	return ret;
}
