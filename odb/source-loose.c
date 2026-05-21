#include "git-compat-util.h"
#include "abspath.h"
#include "chdir-notify.h"
#include "loose.h"
#include "odb.h"
#include "odb/source-files.h"
#include "odb/source-loose.h"
#include "oidtree.h"

static void odb_source_loose_clear_cache(struct odb_source_loose *loose)
{
	oidtree_clear(loose->cache);
	FREE_AND_NULL(loose->cache);
	memset(&loose->subdir_seen, 0,
	       sizeof(loose->subdir_seen));
}

static void odb_source_loose_reprepare(struct odb_source *source)
{
	struct odb_source_loose *loose = odb_source_loose_downcast(source);
	odb_source_loose_clear_cache(loose);
}

static void odb_source_loose_reparent(const char *name UNUSED,
				      const char *old_cwd,
				      const char *new_cwd,
				      void *cb_data)
{
	struct odb_source_loose *loose = cb_data;
	char *path = reparent_relative_path(old_cwd, new_cwd,
					    loose->base.path);
	free(loose->base.path);
	loose->base.path = path;
}

static void odb_source_loose_free(struct odb_source *source)
{
	struct odb_source_loose *loose = odb_source_loose_downcast(source);
	odb_source_loose_clear_cache(loose);
	loose_object_map_clear(&loose->map);
	chdir_notify_unregister(NULL, odb_source_loose_reparent, loose);
	odb_source_release(&loose->base);
	free(loose);
}

struct odb_source_loose *odb_source_loose_new(struct odb_source_files *files)
{
	struct odb_source_loose *loose;

	CALLOC_ARRAY(loose, 1);
	odb_source_init(&loose->base, files->base.odb, ODB_SOURCE_LOOSE,
			files->base.path, files->base.local);
	loose->files = files;

	loose->base.free = odb_source_loose_free;
	loose->base.reprepare = odb_source_loose_reprepare;

	if (!is_absolute_path(loose->base.path))
		chdir_notify_register(NULL, odb_source_loose_reparent, loose);

	return loose;
}
