#include "git-compat-util.h"
#include "odb/source-loose.h"

struct odb_source_loose *odb_source_loose_new(struct odb_source *source)
{
	struct odb_source_loose *loose;
	CALLOC_ARRAY(loose, 1);
	loose->source = source;
	return loose;
}
