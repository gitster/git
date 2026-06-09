#include "git-compat-util.h"
#include "abspath.h"
#include "chdir-notify.h"
#include "midx.h"
#include "odb/source-packed.h"
#include "packfile.h"

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

	if (!is_absolute_path(parent->base.path))
		chdir_notify_register(NULL, odb_source_packed_reparent, packed);

	return packed;
}
