#ifndef OBJECT_STORE_H
#define OBJECT_STORE_H

#include "khashl.h"
#include "dir.h"
#include "object-store-ll.h"

KHASHL_MAP_INIT(KH_LOCAL, odb_path_map, odb_path_map,
	const char * /* key: odb_path */, struct object_directory *,
	fspathhash, fspatheq)

#endif /* OBJECT_STORE_H */
