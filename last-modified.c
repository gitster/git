#include "git-compat-util.h"
#include "bloom.h"
#include "commit-graph.h"
#include "commit.h"
#include "diff.h"
#include "diffcore.h"
#include "dir.h"
#include "last-modified.h"
#include "log-tree.h"
#include "object.h"
#include "repository.h"
#include "revision.h"

struct last_modified_entry {
	struct hashmap_entry hashent;
	struct object_id oid;
	struct bloom_key key;
	const char path[FLEX_ARRAY];
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
		if (lm->rev.bloom_filter_settings)
			fill_bloom_key(path, strlen(path), &ent->key,
				       lm->rev.bloom_filter_settings);
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
			return error(_("can only get last-modified one tree at a time"));

		diff_tree_oid(lm->rev.repo->hash_algo->empty_tree,
			      &obj->item->oid, "", &diffopt);
		diff_flush(&diffopt);
	}
	diff_free(&diffopt);

	return 0;
}

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

int last_modified_init(struct last_modified *lm, struct repository *r,
		       const char *prefix, int argc, const char **argv)
{
	memset(lm, 0, sizeof(*lm));
	hashmap_init(&lm->paths, last_modified_entry_hashcmp, NULL, 0);

	repo_init_revisions(r, &lm->rev, prefix);
	lm->rev.def = "HEAD";
	lm->rev.combine_merges = 1;
	lm->rev.show_root_diff = 1;
	lm->rev.boundary = 1;
	lm->rev.no_commit_id = 1;
	lm->rev.diff = 1;
	if (setup_revisions(argc, argv, &lm->rev, NULL) > 1)
		return error(_("unknown last-modified argument: %s"), argv[1]);

	/*
	 * We're not interested in generation numbers here,
	 * but calling this function to prepare the commit-graph.
	 */
	(void)generation_numbers_enabled(lm->rev.repo);
	lm->rev.bloom_filter_settings = get_bloom_filter_settings(lm->rev.repo);

	if (populate_paths_from_revs(lm) < 0)
		return error(_("unable to setup last-modified"));

	return 0;
}

void last_modified_release(struct last_modified *lm)
{
	struct hashmap_iter iter;
	struct last_modified_entry *ent;

	hashmap_for_each_entry (&lm->paths, &iter, ent, hashent)
		clear_bloom_key(&ent->key);

	hashmap_clear_and_free(&lm->paths, struct last_modified_entry, hashent);
	release_revisions(&lm->rev);
}

struct last_modified_callback_data {
	struct commit *commit;
	struct hashmap *paths;

	last_modified_callback callback;
	void *callback_data;
};

static void mark_path(const char *path, const struct object_id *oid,
		      struct last_modified_callback_data *data)
{
	struct last_modified_entry *ent;

	/* Is it even a path that we are interested in? */
	ent = hashmap_get_entry_from_hash(data->paths, strhash(path), path,
					  struct last_modified_entry, hashent);
	if (!ent)
		return;

	/*
	 * Is it arriving at a version of interest, or is it from a side branch
	 * which did not contribute to the final state?
	 */
	if (!oideq(oid, &ent->oid))
		return;

	if (data->callback)
		data->callback(path, data->commit, data->callback_data);

	hashmap_remove(data->paths, &ent->hashent, path);
	clear_bloom_key(&ent->key);
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
			 * a final path/sha1 state. Note that this covers some
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

static int maybe_changed_path(struct last_modified *lm, struct commit *origin)
{
	struct bloom_filter *filter;
	struct last_modified_entry *ent;
	struct hashmap_iter iter;

	if (!lm->rev.bloom_filter_settings)
		return 1;

	filter = get_bloom_filter(lm->rev.repo, origin);
	if (!filter)
		return 1;

	hashmap_for_each_entry (&lm->paths, &iter, ent, hashent) {
		if (bloom_filter_contains(filter, &ent->key,
					  lm->rev.bloom_filter_settings))
			return 1;
	}
	return 0;
}

int last_modified_run(struct last_modified *lm, last_modified_callback cb,
		      void *cbdata)
{
	struct last_modified_callback_data data;

	data.paths = &lm->paths;
	data.callback = cb;
	data.callback_data = cbdata;

	lm->rev.diffopt.output_format = DIFF_FORMAT_CALLBACK;
	lm->rev.diffopt.format_callback = last_modified_diff;
	lm->rev.diffopt.format_callback_data = &data;

	prepare_revision_walk(&lm->rev);

	while (hashmap_get_size(&lm->paths)) {
		data.commit = get_revision(&lm->rev);
		if (!data.commit)
			break;

		if (!maybe_changed_path(lm, data.commit))
			continue;

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
