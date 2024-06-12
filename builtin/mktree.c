/*
 * GIT - the stupid content tracker
 *
 * Copyright (c) Junio C Hamano, 2006, 2009
 */
#include "builtin.h"
#include "cache-tree.h"
#include "gettext.h"
#include "hex.h"
#include "index-info.h"
#include "quote.h"
#include "read-cache-ll.h"
#include "strbuf.h"
#include "tree.h"
#include "object-name.h"
#include "parse-options.h"
#include "pathspec.h"
#include "object-store-ll.h"

struct tree_entry {
	struct hashmap_entry ent;

	/* Internal */
	size_t order;
	int expand_dir;

	unsigned mode;
	struct object_id oid;
	int len;
	char name[FLEX_ARRAY];
};

static inline size_t df_path_len(size_t pathlen, unsigned int mode)
{
	return (S_ISDIR(mode) || !mode) ? pathlen - 1 : pathlen;
}

struct tree_entry_array {
	size_t nr, alloc;
	struct tree_entry **entries;

	struct hashmap df_name_hash;
	int has_nested_entries;
};

static int df_name_hash_cmp(const void *cmp_data UNUSED,
			    const struct hashmap_entry *eptr,
			    const struct hashmap_entry *entry_or_key,
			    const void *keydata UNUSED)
{
	const struct tree_entry *e1, *e2;
	size_t e1_len, e2_len;

	e1 = container_of(eptr, const struct tree_entry, ent);
	e2 = container_of(entry_or_key, const struct tree_entry, ent);

	e1_len = df_path_len(e1->len, e1->mode);
	e2_len = df_path_len(e2->len, e2->mode);

	return e1_len != e2_len ||
	       name_compare(e1->name, e1_len, e2->name, e2_len);
}

static void init_tree_entry_array(struct tree_entry_array *arr)
{
	hashmap_init(&arr->df_name_hash, df_name_hash_cmp, NULL, 0);
}

static void tree_entry_array_push(struct tree_entry_array *arr, struct tree_entry *ent)
{
	ALLOC_GROW(arr->entries, arr->nr + 1, arr->alloc);
	arr->entries[arr->nr++] = ent;
}

static struct tree_entry *tree_entry_array_pop(struct tree_entry_array *arr)
{
	if (!arr->nr)
		return NULL;
	return arr->entries[--arr->nr];
}

static void clear_tree_entry_array(struct tree_entry_array *arr)
{
	hashmap_clear(&arr->df_name_hash);
	for (size_t i = 0; i < arr->nr; i++)
		FREE_AND_NULL(arr->entries[i]);
	arr->nr = 0;
}

static void release_tree_entry_array(struct tree_entry_array *arr)
{
	hashmap_clear(&arr->df_name_hash);
	FREE_AND_NULL(arr->entries);
	arr->nr = arr->alloc = 0;
}

static void append_to_tree(unsigned mode, struct object_id *oid, const char *path,
			   struct tree_entry_array *arr, int literally)
{
	struct tree_entry *ent;
	size_t len = strlen(path);

	if (literally) {
		FLEX_ALLOC_MEM(ent, name, path, len);
	} else {
		size_t len_to_copy = len;

		/* Normalize and validate entry path */
		if (S_ISDIR(mode) || !mode) {
			while(len_to_copy > 0 && is_dir_sep(path[len_to_copy - 1]))
				len_to_copy--;
			len = len_to_copy + 1; /* add space for trailing slash */
		}
		ent = xcalloc(1, st_add3(sizeof(struct tree_entry), len, 1));
		memcpy(ent->name, path, len_to_copy);

		if (!verify_path(ent->name, mode))
			die(_("invalid path '%s'"), path);

		/* mark has_nested_entries if needed */
		if (!arr->has_nested_entries && strchr(ent->name, '/'))
			arr->has_nested_entries = 1;

		/* Add trailing slash to dir */
		if (S_ISDIR(mode) || !mode)
			ent->name[len - 1] = '/';
	}

	ent->mode = mode;
	ent->len = len;
	oidcpy(&ent->oid, oid);

	/* Append the update */
	ent->order = arr->nr;
	tree_entry_array_push(arr, ent);
}

static int ent_compare(const void *a_, const void *b_, void *ctx)
{
	int cmp;
	struct tree_entry *a = *(struct tree_entry **)a_;
	struct tree_entry *b = *(struct tree_entry **)b_;
	int ignore_mode = *((int *)ctx);

	size_t a_len = a->len, b_len = b->len;

	if (ignore_mode) {
		a_len = df_path_len(a_len, a->mode);
		b_len = df_path_len(b_len, b->mode);
	}

	cmp = name_compare(a->name, a_len, b->name, b_len);
	return cmp ? cmp : b->order - a->order;
}

static void sort_and_dedup_tree_entry_array(struct tree_entry_array *arr)
{
	size_t count = arr->nr;
	struct tree_entry *prev = NULL;

	int ignore_mode = 1;
	QSORT_S(arr->entries, arr->nr, ent_compare, &ignore_mode);

	arr->nr = 0;
	for (size_t i = 0; i < count; i++) {
		struct tree_entry *curr = arr->entries[i];
		if (prev &&
		    !name_compare(prev->name, df_path_len(prev->len, prev->mode),
				  curr->name, df_path_len(curr->len, curr->mode))) {
			FREE_AND_NULL(curr);
		} else {
			arr->entries[arr->nr++] = curr;
			prev = curr;
		}
	}

	/* Sort again to order the entries for tree insertion */
	ignore_mode = 0;
	QSORT_S(arr->entries, arr->nr, ent_compare, &ignore_mode);

	if (arr->has_nested_entries) {
		struct tree_entry_array parent_dir_ents = { 0 };

		count = arr->nr;
		arr->nr = 0;

		/* Remove any entries where one of its parent dirs has a higher 'order' */
		for (size_t i = 0; i < count; i++) {
			const char *skipped_prefix;
			struct tree_entry *parent;
			struct tree_entry *curr = arr->entries[i];
			int skip_entry = 0;

			while ((parent = tree_entry_array_pop(&parent_dir_ents))) {
				if (!skip_prefix(curr->name, parent->name, &skipped_prefix))
					continue;

				/* entry in dir, so we push the parent back onto the stack */
				tree_entry_array_push(&parent_dir_ents, parent);

				if (parent->order > curr->order)
					skip_entry = 1;
				else
					parent->expand_dir = 1;

				break;
			}

			if (!skip_entry) {
				arr->entries[arr->nr++] = curr;
				if (S_ISDIR(curr->mode) || !curr->mode)
					tree_entry_array_push(&parent_dir_ents, curr);
			} else {
				FREE_AND_NULL(curr);
			}
		}

		release_tree_entry_array(&parent_dir_ents);
	}

	/* Finally, initialize the directory-file conflict hash map */
	for (size_t i = 0; i < count; i++) {
		struct tree_entry *curr = arr->entries[i];
		hashmap_entry_init(&curr->ent,
				   memhash(curr->name, df_path_len(curr->len, curr->mode)));
		hashmap_put(&arr->df_name_hash, &curr->ent);
	}
}

struct tree_entry_iterator {
	struct tree_entry *current;

	/* private */
	struct {
		struct tree_entry_array *arr;
		size_t idx;
	} priv;
};

static void init_tree_entry_iterator(struct tree_entry_iterator *iter,
				     struct tree_entry_array *arr)
{
	iter->priv.arr = arr;
	iter->priv.idx = 0;
	iter->current = 0 < arr->nr ? arr->entries[0] : NULL;
}

/*
 * Advance the tree entry iterator to the next entry in the array. If no entries
 * remain, 'current' is set to NULL. Returns the previous 'current' value of the
 * iterator.
 */
static struct tree_entry *advance_tree_entry_iterator(struct tree_entry_iterator *iter)
{
	struct tree_entry *prev = iter->current;
	iter->current = (iter->priv.idx + 1) < iter->priv.arr->nr
			? iter->priv.arr->entries[++iter->priv.idx]
			: NULL;
	return prev;
}

struct build_index_data {
	struct tree_entry_iterator iter;
	struct hashmap *df_name_hash;
	struct index_state istate;
};

static int build_index_from_tree(const struct object_id *oid,
				 struct strbuf *base, const char *filename,
				 unsigned mode, void *context);

static int add_tree_entry_to_index(struct build_index_data *data,
				   struct tree_entry *ent)
{
	if (!ent->mode)
		return 0;

	if (ent->expand_dir) {
		int ret = 0;
		struct pathspec ps = { 0 };
		struct tree *subtree = parse_tree_indirect(&ent->oid);
		struct strbuf base_path = STRBUF_INIT;
		strbuf_add(&base_path, ent->name, ent->len);

		if (!subtree)
			ret = error(_("not a tree object: %s"), oid_to_hex(&ent->oid));
		else if (read_tree_at(the_repository, subtree, &base_path, 0, &ps,
				 build_index_from_tree, data) < 0)
			ret = -1;

		strbuf_release(&base_path);
		if (ret)
			return ret;

	} else {
		struct cache_entry *ce = make_cache_entry(&data->istate,
							  ent->mode, &ent->oid,
							  ent->name, 0, 0);
		if (!ce)
			return error(_("make_cache_entry failed for path '%s'"), ent->name);

		add_index_entry(&data->istate, ce, ADD_CACHE_JUST_APPEND);
	}

	return 0;
}

static int build_index_from_tree(const struct object_id *oid,
				 struct strbuf *base, const char *filename,
				 unsigned mode, void *context)
{
	int result;
	struct tree_entry *base_tree_ent;
	struct build_index_data *cbdata = context;
	size_t filename_len = strlen(filename);
	size_t path_len = S_ISDIR(mode) ? st_add3(filename_len, base->len, 1)
					: st_add(filename_len, base->len);

	/* Create a tree entry from the current entry in read_tree iteration */
	base_tree_ent = xcalloc(1, st_add3(sizeof(struct tree_entry), path_len, 1));
	base_tree_ent->len = path_len;
	base_tree_ent->mode = mode;
	oidcpy(&base_tree_ent->oid, oid);

	memcpy(base_tree_ent->name, base->buf, base->len);
	memcpy(base_tree_ent->name + base->len, filename, filename_len);
	if (S_ISDIR(mode))
		base_tree_ent->name[base_tree_ent->len - 1] = '/';

	while (cbdata->iter.current) {
		const char *skipped_prefix;
		struct tree_entry *ent = cbdata->iter.current;
		int cmp;

		cmp = name_compare(ent->name, ent->len,
				   base_tree_ent->name, base_tree_ent->len);
		if (!cmp || cmp < 0) {
			advance_tree_entry_iterator(&cbdata->iter);

			if (add_tree_entry_to_index(cbdata, ent) < 0) {
				result = error(_("failed to add tree entry '%s'"), ent->name);
				goto cleanup_and_return;
			}

			if (!cmp) {
				result = 0;
				goto cleanup_and_return;
			} else
				continue;
		} else if (skip_prefix(ent->name, base_tree_ent->name, &skipped_prefix) &&
			   S_ISDIR(base_tree_ent->mode)) {
			/* The entry is in the current traversed tree entry, so we recurse */
			result = READ_TREE_RECURSIVE;
			goto cleanup_and_return;
		}

		break;
	}

	/*
	 * If the tree entry should be replaced with an entry with the same name
	 * (but different mode), skip it.
	 */
	hashmap_entry_init(&base_tree_ent->ent,
			   memhash(base_tree_ent->name, df_path_len(base_tree_ent->len, base_tree_ent->mode)));
	if (hashmap_get_entry(cbdata->df_name_hash, base_tree_ent, ent, NULL)) {
		result = 0;
		goto cleanup_and_return;
	}

	if (add_tree_entry_to_index(cbdata, base_tree_ent)) {
		result = -1;
		goto cleanup_and_return;
	}

	result = 0;

cleanup_and_return:
	FREE_AND_NULL(base_tree_ent);
	return result;
}

static void write_tree(struct tree_entry_array *arr, struct tree *base_tree,
		       struct object_id *oid)
{
	struct build_index_data cbdata = { 0 };
	struct tree_entry *ent;
	struct pathspec ps = { 0 };

	sort_and_dedup_tree_entry_array(arr);

	index_state_init(&cbdata.istate, the_repository);
	cbdata.istate.sparse_index = 1;
	init_tree_entry_iterator(&cbdata.iter, arr);
	cbdata.df_name_hash = &arr->df_name_hash;

	/* Construct an in-memory index from the provided entries & base tree */
	if (base_tree &&
	    read_tree(the_repository, base_tree, &ps, build_index_from_tree, &cbdata) < 0)
		die(_("failed to create tree"));

	while ((ent = advance_tree_entry_iterator(&cbdata.iter))) {
		if (add_tree_entry_to_index(&cbdata, ent))
			die(_("failed to add tree entry '%s'"), ent->name);
	}

	/* Write out new tree */
	if (cache_tree_update(&cbdata.istate, WRITE_TREE_SILENT | WRITE_TREE_MISSING_OK))
		die(_("failed to write tree"));
	oidcpy(oid, &cbdata.istate.cache_tree->oid);

	release_index(&cbdata.istate);
}

static void write_tree_literally(struct tree_entry_array *arr,
				 struct object_id *oid)
{
	struct strbuf buf;
	size_t size = 0;

	for (size_t i = 0; i < arr->nr; i++)
		size += 32 + arr->entries[i]->len;

	strbuf_init(&buf, size);
	for (size_t i = 0; i < arr->nr; i++) {
		struct tree_entry *ent = arr->entries[i];
		strbuf_addf(&buf, "%o %s%c", ent->mode, ent->name, '\0');
		strbuf_add(&buf, ent->oid.hash, the_hash_algo->rawsz);
	}

	write_object_file(buf.buf, buf.len, OBJ_TREE, oid);
	strbuf_release(&buf);
}

static const char *mktree_usage[] = {
	"git mktree [-z] [--missing] [--literally] [--batch] [--] [<tree-ish>]",
	NULL
};

struct mktree_line_data {
	struct tree_entry_array *arr;
	int allow_missing;
	int literally;
};

static int mktree_line(unsigned int mode, struct object_id *oid,
		       enum object_type obj_type, int stage UNUSED,
		       const char *path, void *cbdata)
{
	struct mktree_line_data *data = cbdata;

	if (mode) {
		struct object_info oi = OBJECT_INFO_INIT;
		enum object_type parsed_obj_type;
		enum object_type mode_type = object_type(mode);

		if (obj_type && mode_type != obj_type)
			die("object type (%s) doesn't match mode type (%s)",
			    type_name(obj_type), type_name(mode_type));

		oi.typep = &parsed_obj_type;

		if (oid_object_info_extended(the_repository, oid, &oi,
					     OBJECT_INFO_LOOKUP_REPLACE |
					     OBJECT_INFO_QUICK |
					     OBJECT_INFO_SKIP_FETCH_OBJECT) < 0)
			parsed_obj_type = -1;

		if (parsed_obj_type < 0) {
			if (data->allow_missing || S_ISGITLINK(mode)) {
				; /* no problem - missing objects & submodules are presumed to be of the right type */
			} else {
				die("entry '%s' object %s is unavailable", path, oid_to_hex(oid));
			}
		} else if (parsed_obj_type != mode_type) {
			/*
			* The object exists but is of the wrong type.
			* This is a problem regardless of allow_missing
			* because the new tree entry will never be correct.
			*/
			die("entry '%s' object %s is a %s but specified type was (%s)",
			path, oid_to_hex(oid), type_name(parsed_obj_type), type_name(mode_type));
		}
	}

	append_to_tree(mode, oid, path, data->arr, data->literally);
	return 0;
}

int cmd_mktree(int ac, const char **av, const char *prefix)
{
	struct object_id oid;
	int nul_term_line = 0;
	int is_batch_mode = 0;
	struct tree_entry_array arr = { 0 };
	struct mktree_line_data mktree_line_data = { .arr = &arr };
	struct tree *base_tree = NULL;
	int ret;

	const struct option option[] = {
		OPT_BOOL('z', NULL, &nul_term_line, N_("input is NUL terminated")),
		OPT_BOOL(0, "missing", &mktree_line_data.allow_missing, N_("allow missing objects")),
		OPT_BOOL(0, "literally", &mktree_line_data.literally,
			 N_("do not sort, deduplicate, or validate paths of tree entries")),
		OPT_BOOL(0, "batch", &is_batch_mode, N_("allow creation of more than one tree")),
		OPT_END()
	};

	ac = parse_options(ac, av, prefix, option, mktree_usage, 0);
	if (ac > 1)
		usage_with_options(mktree_usage, option);

	if (ac) {
		struct object_id base_tree_oid;

		if (mktree_line_data.literally)
			die(_("option '%s' and tree-ish cannot be used together"), "--literally");

		if (repo_get_oid(the_repository, av[0], &base_tree_oid))
			die(_("not a valid object name %s"), av[0]);

		base_tree = parse_tree_indirect(&base_tree_oid);
		if (!base_tree)
			die(_("not a tree object: %s"), oid_to_hex(&base_tree_oid));
	}

	init_tree_entry_array(&arr);

	do {
		ret = read_index_info(nul_term_line, mktree_line, &mktree_line_data);
		if (ret < 0)
			break;

		/* empty lines denote tree boundaries in batch mode */
		if (ret > 0 && !is_batch_mode)
			die("input format error: (blank line only valid in batch mode)");

		if (is_batch_mode && !ret && arr.nr < 1) {
			/*
			 * Execution gets here if the last tree entry is terminated with a
			 * new-line.  The final new-line has been made optional to be
			 * consistent with the original non-batch behaviour of mktree.
			 */
			; /* skip creating an empty tree */
		} else {
			if (mktree_line_data.literally)
				write_tree_literally(&arr, &oid);
			else
				write_tree(&arr, base_tree, &oid);
			puts(oid_to_hex(&oid));
			fflush(stdout);
		}
		clear_tree_entry_array(&arr); /* reset tree entry buffer for re-use in batch mode */
	} while (ret > 0);

	release_tree_entry_array(&arr);
	return !!ret;
}
