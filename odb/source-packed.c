#include "git-compat-util.h"
#include "odb/source-packed.h"

struct odb_source_packed *odb_source_packed_new(struct odb_source *source)
{
	struct odb_source_packed *store;
	CALLOC_ARRAY(store, 1);
	store->source = source;
	strmap_init(&store->packs_by_path);
	return store;
}
