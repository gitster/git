#include "git-compat-util.h"
#include "abspath.h"
#include "chdir-notify.h"
#include "dir.h"
#include "git-zlib.h"
#include "mergesort.h"
#include "midx.h"
#include "odb/source-packed.h"
#include "odb/streaming.h"
#include "packfile.h"

int find_pack_entry(struct odb_source_packed *store,
		    const struct object_id *oid,
		    struct pack_entry *e)
{
	struct packfile_list_entry *l;

	odb_source_packed_prepare(store);
	if (store->midx && fill_midx_entry(store->midx, oid, e))
		return 1;

	for (l = store->packs.head; l; l = l->next) {
		struct packed_git *p = l->pack;

		if (!p->multi_pack_index && packfile_fill_entry(p, oid, e)) {
			if (!store->skip_mru_updates)
				packfile_list_prepend(&store->packs, p);
			return 1;
		}
	}

	return 0;
}

static int odb_source_packed_read_object_info(struct odb_source *source,
					      const struct object_id *oid,
					      struct object_info *oi,
					      enum object_info_flags flags)
{
	struct odb_source_packed *packed = odb_source_packed_downcast(source);
	struct pack_entry e;
	int ret;

	/*
	 * In case the first read didn't surface the object, we have to reload
	 * packfiles. This may cause us to discover new packfiles that have
	 * been added since the last time we have prepared the packfile store.
	 */
	if (flags & OBJECT_INFO_SECOND_READ)
		odb_source_reprepare(source);

	if (!find_pack_entry(packed, oid, &e))
		return 1;

	/*
	 * We know that the caller doesn't actually need the
	 * information below, so return early.
	 */
	if (!oi)
		return 0;

	ret = packed_object_info(e.p, e.offset, oi);
	if (ret < 0) {
		mark_bad_packed_object(e.p, oid);
		return -1;
	}

	return 0;
}

static int odb_source_packed_read_object_stream(struct odb_read_stream **out,
						struct odb_source *source,
						const struct object_id *oid)
{
	struct odb_source_packed *packed = odb_source_packed_downcast(source);
	struct pack_entry e;

	if (!find_pack_entry(packed, oid, &e))
		return -1;

	return packfile_read_object_stream(out, oid, e.p, e.offset);
}

void (*report_garbage)(unsigned seen_bits, const char *path);

static void report_helper(const struct string_list *list,
			  int seen_bits, int first, int last)
{
	if (seen_bits == (PACKDIR_FILE_PACK|PACKDIR_FILE_IDX))
		return;

	for (; first < last; first++)
		report_garbage(seen_bits, list->items[first].string);
}

static void report_pack_garbage(struct string_list *list)
{
	int baselen = -1, first = 0, seen_bits = 0;

	if (!report_garbage)
		return;

	string_list_sort(list);

	for (size_t i = 0; i < list->nr; i++) {
		const char *path = list->items[i].string;
		if (baselen != -1 &&
		    strncmp(path, list->items[first].string, baselen)) {
			report_helper(list, seen_bits, first, i);
			baselen = -1;
			seen_bits = 0;
		}
		if (baselen == -1) {
			const char *dot = strrchr(path, '.');
			if (!dot) {
				report_garbage(PACKDIR_FILE_GARBAGE, path);
				continue;
			}
			baselen = dot - path + 1;
			first = i;
		}
		if (!strcmp(path + baselen, "pack"))
			seen_bits |= 1;
		else if (!strcmp(path + baselen, "idx"))
			seen_bits |= 2;
	}
	report_helper(list, seen_bits, first, list->nr);
}

struct prepare_pack_data {
	struct odb_source *source;
	struct string_list *garbage;
};

static void prepare_pack(const char *full_name, size_t full_name_len,
			 const char *file_name, void *_data)
{
	struct prepare_pack_data *data = (struct prepare_pack_data *)_data;
	struct odb_source_files *files = odb_source_files_downcast(data->source);
	size_t base_len = full_name_len;

	if (strip_suffix_mem(full_name, &base_len, ".idx") &&
	    !(files->packed->midx &&
	      midx_contains_pack(files->packed->midx, file_name))) {
		char *trimmed_path = xstrndup(full_name, full_name_len);
		packfile_store_load_pack(files->packed,
					 trimmed_path, data->source->local);
		free(trimmed_path);
	}

	if (!report_garbage)
		return;

	if (!strcmp(file_name, "multi-pack-index") ||
	    !strcmp(file_name, "multi-pack-index.d"))
		return;
	if (starts_with(file_name, "multi-pack-index") &&
	    (ends_with(file_name, ".bitmap") || ends_with(file_name, ".rev")))
		return;
	if (ends_with(file_name, ".idx") ||
	    ends_with(file_name, ".rev") ||
	    ends_with(file_name, ".pack") ||
	    ends_with(file_name, ".bitmap") ||
	    ends_with(file_name, ".keep") ||
	    ends_with(file_name, ".promisor") ||
	    ends_with(file_name, ".mtimes"))
		string_list_append(data->garbage, full_name);
	else
		report_garbage(PACKDIR_FILE_GARBAGE, full_name);
}

static void prepare_packed_git_one(struct odb_source *source)
{
	struct string_list garbage = STRING_LIST_INIT_DUP;
	struct prepare_pack_data data = {
		.source = source,
		.garbage = &garbage,
	};

	for_each_file_in_pack_dir(source->path, prepare_pack, &data);

	report_pack_garbage(data.garbage);
	string_list_clear(data.garbage, 0);
}

DEFINE_LIST_SORT(static, sort_packs, struct packfile_list_entry, next);

static int sort_pack(const struct packfile_list_entry *a,
		     const struct packfile_list_entry *b)
{
	int st;

	/*
	 * Local packs tend to contain objects specific to our
	 * variant of the project than remote ones.  In addition,
	 * remote ones could be on a network mounted filesystem.
	 * Favor local ones for these reasons.
	 */
	st = a->pack->pack_local - b->pack->pack_local;
	if (st)
		return -st;

	/*
	 * Younger packs tend to contain more recent objects,
	 * and more recent objects tend to get accessed more
	 * often.
	 */
	if (a->pack->mtime < b->pack->mtime)
		return 1;
	else if (a->pack->mtime == b->pack->mtime)
		return 0;
	return -1;
}

void odb_source_packed_prepare(struct odb_source_packed *source)
{
	if (source->initialized)
		return;

	prepare_multi_pack_index_one(&source->files->base);
	prepare_packed_git_one(&source->files->base);

	sort_packs(&source->packs.head, sort_pack);
	for (struct packfile_list_entry *e = source->packs.head; e; e = e->next)
		if (!e->next)
			source->packs.tail = e;

	source->initialized = true;
}

static void odb_source_packed_reprepare(struct odb_source *source)
{
	struct odb_source_packed *packed = odb_source_packed_downcast(source);
	packed->initialized = false;
	odb_source_packed_prepare(packed);
}

static void odb_source_packed_reparent(const char *name UNUSED,
				       const char *old_cwd,
				       const char *new_cwd,
				       void *cb_data)
{
	struct odb_source_packed *packed = cb_data;
	char *path = reparent_relative_path(old_cwd, new_cwd,
					    packed->base.path);
	free(packed->base.path);
	packed->base.path = path;
}

static void odb_source_packed_close(struct odb_source *source)
{
	struct odb_source_packed *packed = odb_source_packed_downcast(source);

	for (struct packfile_list_entry *e = packed->packs.head; e; e = e->next) {
		if (e->pack->do_not_close)
			BUG("want to close pack marked 'do-not-close'");
		close_pack(e->pack);
	}
	if (packed->midx)
		close_midx(packed->midx);
	packed->midx = NULL;
}

static void odb_source_packed_free(struct odb_source *source)
{
	struct odb_source_packed *packed = odb_source_packed_downcast(source);

	chdir_notify_unregister(NULL, odb_source_packed_reparent, packed);

	for (struct packfile_list_entry *e = packed->packs.head; e; e = e->next)
		free(e->pack);
	packfile_list_clear(&packed->packs);

	strmap_clear(&packed->packs_by_path, 0);
	odb_source_release(&packed->base);
	free(packed);
}

struct odb_source_packed *odb_source_packed_new(struct odb_source_files *parent)
{
	struct odb_source_packed *packed;

	CALLOC_ARRAY(packed, 1);
	odb_source_init(&packed->base, parent->base.odb, ODB_SOURCE_PACKED,
			parent->base.path, parent->base.local);
	packed->files = parent;
	strmap_init(&packed->packs_by_path);

	packed->base.free = odb_source_packed_free;
	packed->base.close = odb_source_packed_close;
	packed->base.reprepare = odb_source_packed_reprepare;
	packed->base.read_object_info = odb_source_packed_read_object_info;
	packed->base.read_object_stream = odb_source_packed_read_object_stream;

	if (!is_absolute_path(parent->base.path))
		chdir_notify_register(NULL, odb_source_packed_reparent, packed);

	return packed;
}
